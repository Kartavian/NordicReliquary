use std::{
    ffi::{CStr, CString},
    fmt::Write as FmtWrite,
    os::raw::{c_char, c_int},
    path::{Path, PathBuf},
    ptr,
};

use libloot::{
    EvalMode, Game, GameType, MergeMode,
    metadata::{
        File, Message, MessageContent, MessageType, PluginCleaningData, PluginMetadata, Tag,
        select_message_content,
    },
};

/// FFI game type matching the C header.
/// Keep discriminant values in sync with `loot_shim.h`.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub enum LootGameType {
    Oblivion = 0,
    Skyrim = 1,
    Fallout3 = 2,
    FalloutNV = 3,
    Fallout4 = 4,
    SkyrimSE = 5,
    Fallout4VR = 6,
    SkyrimVR = 7,
    Morrowind = 8,
    Starfield = 9,
    OpenMW = 10,
    OblivionRemastered = 11,
}

fn map_game_type(game: LootGameType) -> GameType {
    match game {
        LootGameType::Oblivion => GameType::Oblivion,
        LootGameType::Skyrim => GameType::Skyrim,
        LootGameType::Fallout3 => GameType::Fallout3,
        LootGameType::FalloutNV => GameType::FalloutNV,
        LootGameType::Fallout4 => GameType::Fallout4,
        LootGameType::SkyrimSE => GameType::SkyrimSE,
        LootGameType::Fallout4VR => GameType::Fallout4VR,
        LootGameType::SkyrimVR => GameType::SkyrimVR,
        LootGameType::Morrowind => GameType::Morrowind,
        LootGameType::Starfield => GameType::Starfield,
        LootGameType::OpenMW => GameType::OpenMW,
        LootGameType::OblivionRemastered => GameType::OblivionRemastered,
    }
}

/// Opaque handle that C / C++ code will hold on to.
/// All fields are Rust-only and never touched from the C side.
pub struct LootGameHandle {
    game: Game,
    /// Nordic Reliquary's virtual Data directory.
    data_path: PathBuf,
    /// Last sorted plugin order (UTF-8 plugin names).
    sorted_plugins: Vec<String>,
}

/// Simple list-of-strings type that matches the C header.
#[repr(C)]
pub struct LootStringList {
    pub items: *mut *mut c_char,
    pub count: usize,
}

impl Default for LootStringList {
    fn default() -> Self {
        LootStringList {
            items: ptr::null_mut(),
            count: 0,
        }
    }
}

/// Helper: convert nullable C string to owned Rust `String`.
unsafe fn cstr_to_string(ptr: *const c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    CStr::from_ptr(ptr).to_string_lossy().into_owned()
}

/// Create a new LOOT game handle.
///
/// `game`         – which game we're working with (SkyrimSE, etc.)
/// `data_path`    – Reliquary's virtual Data directory
/// `install_path` – real game install directory
///                  (e.g. `/mnt/skyrimae/SteamLibrary/steamapps/common/Skyrim`)
#[no_mangle]
pub extern "C" fn loot_create_game_handle(
    game: LootGameType,
    data_path: *const c_char,
    install_path: *const c_char,
) -> *mut LootGameHandle {
    // Safety: both pointers come from C and are treated as optional.
    let data = unsafe { cstr_to_string(data_path) };
    let install = unsafe { cstr_to_string(install_path) };

    if install.is_empty() {
        // We can't do anything without a real game directory.
        return ptr::null_mut();
    }

    let game_type = map_game_type(game);

    let game_path = Path::new(&install);
    let local_path = if data.is_empty() {
        // It's fine if the local path doesn't exist; LOOT's tests allow that.
        game_path
    } else {
        Path::new(&data)
    };

    // Initialise libloot's Game, telling it where the install *and* "local" data live.
    // The local path does not need to exist – we are not relying on plugins.txt.
    let mut loot_game = match Game::with_local_path(game_type, game_path, local_path) {
        Ok(g) => g,
        Err(_) => {
            // On failure, just return null; caller can treat this as "LOOT unavailable".
            return ptr::null_mut();
        }
    };

    // Very important: point LOOT at Nordic Reliquary's virtual Data folder so it
    // finds plugins and archives there instead of only under `<Skyrim>/Data`.
    if !data.is_empty() {
        let _ = loot_game.set_additional_data_paths(vec![PathBuf::from(&data)]);
    }

    let handle = LootGameHandle {
        game: loot_game,
        data_path: PathBuf::from(if data.is_empty() { install } else { data }),
        sorted_plugins: Vec::new(),
    };

    Box::into_raw(Box::new(handle))
}

/// Destroy a previously-created game handle.
#[no_mangle]
pub extern "C" fn loot_destroy_game_handle(handle: *mut LootGameHandle) {
    if handle.is_null() {
        return;
    }
    unsafe {
        // Take back ownership and let it drop.
        let _ = Box::from_raw(handle);
    }
}

/// Scan the Reliquary virtual Data folder, load plugin headers into libloot,
/// run LOOT's sorter, and cache the resulting order on the handle.
///
/// Return codes (negative = error, 0 = success):
///   0  – success
///  -1  – null handle
///  -2  – failed to read virtual Data directory
///  -3  – failed to load plugin headers
///  -4  – LOOT sorting failed
#[no_mangle]
pub extern "C" fn loot_sort_plugins(handle: *mut LootGameHandle) -> c_int {
    if handle.is_null() {
        return -1;
    }

    // Safety: caller must give us a valid handle created by `loot_create_game_handle`.
    let handle = unsafe { &mut *handle };

    handle.sorted_plugins.clear();

    // 1. Discover plugins under the Reliquary virtual Data folder.
    let data_path = &handle.data_path;
    let read_dir = match std::fs::read_dir(data_path) {
        Ok(rd) => rd,
        Err(_) => return -2,
    };

    let mut plugin_paths: Vec<PathBuf> = Vec::new();
    let mut plugin_names: Vec<String> = Vec::new();

    for entry in read_dir.flatten() {
        let path = entry.path();
        if !path.is_file() {
            continue;
        }

        if let Some(name) = path.file_name().and_then(|n| n.to_str()) {
            let lower = name.to_ascii_lowercase();
            if lower.ends_with(".esm") || lower.ends_with(".esp") || lower.ends_with(".esl") {
                plugin_paths.push(path.clone());
                plugin_names.push(name.to_string());
            }
        }
    }

    if plugin_paths.is_empty() {
        // Nothing to sort – treat as success.
        return 0;
    }

    // 2. Ask libloot to load just the plugin headers – enough for dependency / metadata sorting.
    let path_refs: Vec<&Path> = plugin_paths.iter().map(|p| p.as_path()).collect();

    if let Err(_) = handle.game.load_plugin_headers(&path_refs) {
        return -3;
    }

    // 3. Feed LOOT our "current" load order (the order we discovered on disk).
    let name_refs: Vec<&str> = plugin_names.iter().map(|s| s.as_str()).collect();

    match handle.game.sort_plugins(&name_refs) {
        Ok(sorted) => {
            handle.sorted_plugins = sorted;
            0
        }
        Err(_) => -4,
    }
}

/// Convert the cached Rust `Vec<String>` of sorted plugin names into a C-friendly
/// `LootStringList { char** items, size_t count }`.
///
/// The caller *must* later call `loot_free_string_list` on the returned list.
#[no_mangle]
pub extern "C" fn loot_get_sorted_plugins(handle: *const LootGameHandle) -> LootStringList {
    if handle.is_null() {
        return LootStringList::default();
    }

    // Safety: handle comes from `loot_create_game_handle`.
    let handle = unsafe { &*handle };

    if handle.sorted_plugins.is_empty() {
        return LootStringList::default();
    }

    // Convert each Rust String into a heap-allocated C string.
    let mut c_strings: Vec<CString> = Vec::with_capacity(handle.sorted_plugins.len());
    for name in &handle.sorted_plugins {
        // Skip any names that contain interior NULs, just in case.
        if let Ok(cs) = CString::new(name.as_str()) {
            c_strings.push(cs);
        }
    }

    if c_strings.is_empty() {
        return LootStringList::default();
    }

    // Turn Vec<CString> into Vec<*mut c_char> while leaking the CStrings themselves.
    let mut raw_items: Vec<*mut c_char> =
        c_strings.into_iter().map(|cs| cs.into_raw()).collect();

    let count = raw_items.len();
    let items_ptr = raw_items.as_mut_ptr();
    std::mem::forget(raw_items);

    LootStringList {
        items: items_ptr,
        count,
    }
}

/// Free a list of C strings that came from `loot_get_sorted_plugins`.
#[no_mangle]
pub extern "C" fn loot_free_string_list(list: LootStringList) {
    if list.items.is_null() || list.count == 0 {
        return;
    }

    unsafe {
        // Rebuild the Vec<*mut c_char> so Rust knows how many pointers to free.
        let items = Vec::from_raw_parts(list.items, list.count, list.count);
        for ptr in items {
            if !ptr.is_null() {
                // Take back ownership of each CString so it gets dropped.
                let _ = CString::from_raw(ptr);
            }
        }
    }
}

#[derive(Default)]
struct FileEntry {
    name: String,
    display: Option<String>,
    detail: Option<String>,
    condition: Option<String>,
    constraint: Option<String>,
}

#[derive(Default)]
struct TagEntry {
    name: String,
    suggestion: &'static str,
    condition: Option<String>,
}

#[derive(Default)]
struct MessageEntry {
    level: &'static str,
    text: String,
    condition: Option<String>,
}

#[derive(Default)]
struct CleaningEntry {
    crc: String,
    itm: u32,
    deleted_references: u32,
    deleted_navmeshes: u32,
    utility: String,
    detail: Option<String>,
}

#[derive(Default)]
struct PluginDetailsPayload {
    name: String,
    group: Option<String>,
    load_after: Vec<FileEntry>,
    requirements: Vec<FileEntry>,
    incompatibilities: Vec<FileEntry>,
    tags: Vec<TagEntry>,
    messages: Vec<MessageEntry>,
    dirty: Vec<CleaningEntry>,
    clean: Vec<CleaningEntry>,
    has_masterlist: bool,
    has_user_metadata: bool,
}

impl PluginDetailsPayload {
    fn new(name: &str, has_masterlist: bool, has_user_metadata: bool) -> Self {
        PluginDetailsPayload {
            name: name.to_owned(),
            has_masterlist,
            has_user_metadata,
            ..Default::default()
        }
    }

    fn apply_metadata(&mut self, metadata: &PluginMetadata) {
        self.group = metadata.group().map(|s| s.to_owned());

        self.load_after = metadata
            .load_after_files()
            .iter()
            .map(file_to_entry)
            .collect();
        self.requirements = metadata
            .requirements()
            .iter()
            .map(file_to_entry)
            .collect();
        self.incompatibilities = metadata
            .incompatibilities()
            .iter()
            .map(file_to_entry)
            .collect();
        self.tags = metadata.tags().iter().map(tag_to_entry).collect();
        self.messages = metadata
            .messages()
            .iter()
            .filter_map(message_entry_from_message)
            .collect();
        self.dirty = metadata
            .dirty_info()
            .iter()
            .map(cleaning_to_entry)
            .collect();
        self.clean = metadata
            .clean_info()
            .iter()
            .map(cleaning_to_entry)
            .collect();
    }
}

fn detail_text(contents: &[MessageContent]) -> Option<String> {
    if contents.is_empty() {
        None
    } else {
        select_message_content(contents, MessageContent::DEFAULT_LANGUAGE)
            .or_else(|| contents.first())
            .map(|c| c.text().to_owned())
    }
}

fn file_to_entry(file: &File) -> FileEntry {
    FileEntry {
        name: file.name().as_str().to_owned(),
        display: file.display_name().map(|s| s.to_owned()),
        detail: detail_text(file.detail()),
        condition: file.condition().map(|s| s.to_owned()),
        constraint: file.constraint().map(|s| s.to_owned()),
    }
}

fn tag_to_entry(tag: &Tag) -> TagEntry {
    TagEntry {
        name: tag.name().to_owned(),
        suggestion: if tag.is_addition() { "add" } else { "remove" },
        condition: tag.condition().map(|s| s.to_owned()),
    }
}

fn message_type_label(kind: MessageType) -> &'static str {
    match kind {
        MessageType::Say => "info",
        MessageType::Warn => "warn",
        MessageType::Error => "error",
    }
}

fn message_entry_from_message(message: &Message) -> Option<MessageEntry> {
    detail_text(message.content()).map(|text| MessageEntry {
        level: message_type_label(message.message_type()),
        text,
        condition: message.condition().map(|s| s.to_owned()),
    })
}

fn cleaning_to_entry(data: &PluginCleaningData) -> CleaningEntry {
    CleaningEntry {
        crc: format!("0x{:08X}", data.crc()),
        itm: data.itm_count(),
        deleted_references: data.deleted_reference_count(),
        deleted_navmeshes: data.deleted_navmesh_count(),
        utility: data.cleaning_utility().to_owned(),
        detail: detail_text(data.detail()),
    }
}

fn escape_json(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for ch in value.chars() {
        match ch {
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            c if c < ' ' => {
                let _ = write!(escaped, "\\u{:04X}", c as u32);
            }
            _ => escaped.push(ch),
        }
    }
    escaped
}

fn append_field_prefix(json: &mut String, first: &mut bool) {
    if !*first {
        json.push(',');
    } else {
        *first = false;
    }
}

fn append_string_field(json: &mut String, key: &str, value: &str, first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":\"");
    json.push_str(&escape_json(value));
    json.push('"');
}

fn append_bool_field(json: &mut String, key: &str, value: bool, first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":");
    json.push_str(if value { "true" } else { "false" });
}

fn append_object_string_field(json: &mut String, key: &str, value: &str, first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":\"");
    json.push_str(&escape_json(value));
    json.push('"');
}

fn append_object_optional_string_field(
    json: &mut String,
    key: &str,
    value: Option<&String>,
    first: &mut bool,
) {
    if let Some(v) = value {
        if !v.is_empty() {
            append_object_string_field(json, key, v, first);
        }
    }
}

fn append_object_number_field(json: &mut String, key: &str, value: u32, first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":");
    let _ = write!(json, "{}", value);
}

fn append_file_entries(json: &mut String, key: &str, items: &[FileEntry], first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":[");
    for (index, item) in items.iter().enumerate() {
        if index > 0 {
            json.push(',');
        }
        json.push('{');
        let mut inner_first = true;
        append_object_string_field(json, "name", &item.name, &mut inner_first);
        append_object_optional_string_field(json, "display", item.display.as_ref(), &mut inner_first);
        append_object_optional_string_field(json, "detail", item.detail.as_ref(), &mut inner_first);
        append_object_optional_string_field(json, "condition", item.condition.as_ref(), &mut inner_first);
        append_object_optional_string_field(json, "constraint", item.constraint.as_ref(), &mut inner_first);
        json.push('}');
    }
    json.push(']');
}

fn append_tag_entries(json: &mut String, key: &str, items: &[TagEntry], first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":[");
    for (index, item) in items.iter().enumerate() {
        if index > 0 {
            json.push(',');
        }
        json.push('{');
        let mut inner_first = true;
        append_object_string_field(json, "name", &item.name, &mut inner_first);
        append_object_string_field(json, "suggestion", item.suggestion, &mut inner_first);
        append_object_optional_string_field(json, "condition", item.condition.as_ref(), &mut inner_first);
        json.push('}');
    }
    json.push(']');
}

fn append_message_entries(json: &mut String, key: &str, items: &[MessageEntry], first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":[");
    for (index, item) in items.iter().enumerate() {
        if index > 0 {
            json.push(',');
        }
        json.push('{');
        let mut inner_first = true;
        append_object_string_field(json, "level", item.level, &mut inner_first);
        append_object_string_field(json, "text", &item.text, &mut inner_first);
        append_object_optional_string_field(json, "condition", item.condition.as_ref(), &mut inner_first);
        json.push('}');
    }
    json.push(']');
}

fn append_cleaning_entries(json: &mut String, key: &str, items: &[CleaningEntry], first: &mut bool) {
    append_field_prefix(json, first);
    json.push('"');
    json.push_str(key);
    json.push_str("\":[");
    for (index, item) in items.iter().enumerate() {
        if index > 0 {
            json.push(',');
        }
        json.push('{');
        let mut inner_first = true;
        append_object_string_field(json, "crc", &item.crc, &mut inner_first);
        append_object_string_field(json, "utility", &item.utility, &mut inner_first);
        append_object_number_field(json, "itm", item.itm, &mut inner_first);
        append_object_number_field(json, "deleted_references", item.deleted_references, &mut inner_first);
        append_object_number_field(json, "deleted_navmeshes", item.deleted_navmeshes, &mut inner_first);
        append_object_optional_string_field(json, "detail", item.detail.as_ref(), &mut inner_first);
        json.push('}');
    }
    json.push(']');
}

impl PluginDetailsPayload {
    fn to_json(&self) -> String {
        let mut json = String::from("{");
        let mut first = true;
        append_string_field(&mut json, "name", &self.name, &mut first);
        append_bool_field(&mut json, "has_masterlist", self.has_masterlist, &mut first);
        append_bool_field(&mut json, "has_user_metadata", self.has_user_metadata, &mut first);
        if let Some(group) = &self.group {
            append_string_field(&mut json, "group", group, &mut first);
        }
        append_file_entries(&mut json, "load_after", &self.load_after, &mut first);
        append_file_entries(&mut json, "requirements", &self.requirements, &mut first);
        append_file_entries(&mut json, "incompatibilities", &self.incompatibilities, &mut first);
        append_tag_entries(&mut json, "tags", &self.tags, &mut first);
        append_message_entries(&mut json, "messages", &self.messages, &mut first);
        append_cleaning_entries(&mut json, "dirty", &self.dirty, &mut first);
        append_cleaning_entries(&mut json, "clean", &self.clean, &mut first);
        json.push('}');
        json
    }
}

fn message_entries_to_json(entries: &[MessageEntry]) -> String {
    let mut json = String::from("[");
    for (index, entry) in entries.iter().enumerate() {
        if index > 0 {
            json.push(',');
        }
        json.push('{');
        let mut inner_first = true;
        append_object_string_field(&mut json, "level", entry.level, &mut inner_first);
        append_object_string_field(&mut json, "text", &entry.text, &mut inner_first);
        append_object_optional_string_field(&mut json, "condition", entry.condition.as_ref(), &mut inner_first);
        json.push('}');
    }
    json.push(']');
    json
}

fn json_string_to_c(json: String) -> *mut c_char {
    match CString::new(json) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn loot_load_masterlist(
    handle: *mut LootGameHandle,
    masterlist_path: *const c_char,
    prelude_path: *const c_char,
) -> c_int {
    if handle.is_null() {
        return -1;
    }

    let handle = unsafe { &mut *handle };
    let masterlist = unsafe { cstr_to_string(masterlist_path) };
    if masterlist.is_empty() {
        return -2;
    }

    let database_handle = handle.game.database();
    let mut database = match database_handle.write() {
        Ok(db) => db,
        Err(_) => return -3,
    };

    let result = if prelude_path.is_null() {
        database.load_masterlist(Path::new(&masterlist))
    } else {
        let prelude = unsafe { cstr_to_string(prelude_path) };
        if prelude.is_empty() {
            database.load_masterlist(Path::new(&masterlist))
        } else {
            database.load_masterlist_with_prelude(
                Path::new(&masterlist),
                Path::new(&prelude),
            )
        }
    };

    match result {
        Ok(_) => 0,
        Err(_) => -4,
    }
}

#[no_mangle]
pub extern "C" fn loot_load_userlist(
    handle: *mut LootGameHandle,
    userlist_path: *const c_char,
) -> c_int {
    if handle.is_null() {
        return -1;
    }

    let handle = unsafe { &mut *handle };
    let userlist = unsafe { cstr_to_string(userlist_path) };
    if userlist.is_empty() {
        return -2;
    }

    let database_handle = handle.game.database();
    let mut database = match database_handle.write() {
        Ok(db) => db,
        Err(_) => return -3,
    };

    match database.load_userlist(Path::new(&userlist)) {
        Ok(_) => 0,
        Err(_) => -4,
    }
}

#[no_mangle]
pub extern "C" fn loot_clear_user_metadata(handle: *mut LootGameHandle) -> c_int {
    if handle.is_null() {
        return -1;
    }

    let handle = unsafe { &mut *handle };
    let database_handle = handle.game.database();
    let mut database = match database_handle.write() {
        Ok(db) => db,
        Err(_) => return -2,
    };

    database.discard_all_user_metadata();
    0
}

#[no_mangle]
pub extern "C" fn loot_get_plugin_details_json(
    handle: *mut LootGameHandle,
    plugin_name: *const c_char,
) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }

    let handle = unsafe { &mut *handle };
    let plugin = unsafe { cstr_to_string(plugin_name) };
    if plugin.is_empty() {
        return ptr::null_mut();
    }

    let database = handle.game.database();
    let guard = match database.read() {
        Ok(guard) => guard,
        Err(_) => return ptr::null_mut(),
    };

    let combined = guard
        .plugin_metadata(&plugin, MergeMode::WithUserMetadata, EvalMode::Evaluate)
        .ok()
        .flatten();

    let has_masterlist = guard
        .plugin_metadata(&plugin, MergeMode::WithoutUserMetadata, EvalMode::Evaluate)
        .ok()
        .flatten()
        .is_some();

    let has_user_metadata = guard
        .plugin_user_metadata(&plugin, EvalMode::Evaluate)
        .ok()
        .flatten()
        .is_some();

    let mut payload = PluginDetailsPayload::new(&plugin, has_masterlist, has_user_metadata);
    if let Some(metadata) = combined.as_ref() {
        payload.apply_metadata(metadata);
    }

    json_string_to_c(payload.to_json())
}

#[no_mangle]
pub extern "C" fn loot_get_general_messages_json(handle: *mut LootGameHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }

    let handle = unsafe { &mut *handle };
    let database = handle.game.database();
    let mut guard = match database.write() {
        Ok(guard) => guard,
        Err(_) => return ptr::null_mut(),
    };

    let messages = match guard.general_messages(EvalMode::Evaluate) {
        Ok(msgs) => msgs,
        Err(_) => return ptr::null_mut(),
    };

    let entries: Vec<MessageEntry> = messages
        .iter()
        .filter_map(message_entry_from_message)
        .collect();

    json_string_to_c(message_entries_to_json(&entries))
}

#[no_mangle]
pub extern "C" fn loot_free_json(json: *mut c_char) {
    if json.is_null() {
        return;
    }

    unsafe {
        let _ = CString::from_raw(json);
    }
}
