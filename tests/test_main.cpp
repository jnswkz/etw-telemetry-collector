// Với Catch2, Catch2WithMain đã cung cấp main().
// File này chỉ cung cấp main() khi build fallback (không có Catch2).
#ifndef ETWC_HAVE_CATCH2
int run_fallback_tests();  // định nghĩa trong các test_*.cpp fallback
int main() { return run_fallback_tests(); }
#endif
