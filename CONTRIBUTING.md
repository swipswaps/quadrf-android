# Contributing to Quad RF Android

Thank you for your interest in contributing to this project! We welcome
contributions of all kinds – code, documentation, bug reports, feature
suggestions, and testing.

## How to Contribute

1. **Fork** the repository on GitHub.
2. **Create a branch** for your feature or fix:
   git checkout -b feature/your-feature-name
3. **Make your changes**. Follow the coding style (see below).
4. **Test** your changes thoroughly.
5. **Commit** with a clear, descriptive message.
6. **Push** your branch and open a Pull Request.

## Coding Standards

### Java/Kotlin
- Follow Android's official style guide:
  source.android.com/docs/source/style-guide
- Use meaningful variable names.
- Add Javadoc comments for public methods.

### C/C++
- Use C++17.
- Follow the Google C++ Style Guide:
  google.github.io/styleguide/cppguide.html
- Use RAII for resource management.
- Avoid raw pointers where possible.

### Comments
- Explain "why", not "what".
- Use verbatim citations where applicable.
- Keep comments up to date with code changes.

### Documentation
- Update README.md for user-facing changes.
- Update API documentation for public interfaces.

## Testing Requirements

- Test with at least one Quad RF board (or compatible SDR).
- Verify no crashes or memory leaks.
- Check that USB permission is requested and handled correctly.
- Test on at least two different Android devices.

## Reporting Issues

- Use the GitHub Issue Tracker.
- Include: device model, Android version, steps to reproduce, and logs.
- Tag issues with appropriate labels (bug, enhancement, question).

## First-Time Contributors

- Look for issues tagged "good first issue" or "help wanted".
- Ask questions in the discussion forum or on the issue itself.
- Don't be shy – we were all beginners once.

## Pull Request Process

1. Ensure your code passes all tests.
2. Update documentation as needed.
3. Your PR will be reviewed by maintainers.
4. Address any feedback promptly.
5. Once approved, your PR will be merged.

## Code of Conduct

This project follows the Contributor Covenant Code of Conduct.
Please read CODE_OF_CONDUCT.md before contributing.

## License

By contributing, you agree that your contributions will be licensed
under the GNU General Public License v3.0.
