// This file provides the __main entry point for the Jaguar C runtime.
// It simply calls the user's main() function.
void __main() {
    // We don't have argc/argv, so pass NULL
    main();
}
