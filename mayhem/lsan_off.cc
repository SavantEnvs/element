// Leak detection is not the bug class this integration fuzzes for (SPEC §6.2 item 15).
// Default visibility: upstream sets CMAKE_CXX_VISIBILITY_PRESET hidden for every target.
extern "C" __attribute__((visibility("default"), used)) int __lsan_is_turned_off() { return 1; }
