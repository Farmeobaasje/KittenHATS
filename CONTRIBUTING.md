# Contributing to KittenHATS

Thank you for your interest in contributing to KittenHATS! This project aims to make a Nintendo Switch safe for children to use, and every contribution helps.

## How to Contribute

### Reporting Bugs

1. Check the [Issues](https://github.com/sthetix/KittenHATS/issues) to see if the bug has already been reported
2. If not, open a new issue using the bug report template
3. Include:
   - Switch model and firmware version
   - HATS version
   - Steps to reproduce
   - Expected vs actual behavior
   - Any relevant logs or photos

### Feature Requests

1. Open a feature request using the feature request template
2. Describe the problem you're trying to solve
3. Explain how the feature would work
4. Note: KittenHATS is a **minimal, focused** project — features that add complexity without clear benefit may be declined

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Make your changes
4. Run `make clean && make` to verify the build
5. Commit with a clear message
6. Push and open a Pull Request

## Development Guidelines

### Code Style

- C99 standard (same as Hekate)
- 4-space indentation
- Follow Hekate naming conventions for low-level code
- Follow LVGL naming conventions for UI code
- No Prettier dependency — use editor-native formatting

### Quality Requirements

- `make clean && make` must pass with zero errors
- No warnings in `app/src/` code
- Vendor warnings (from `app/vendor/hekate/`) are acceptable but should be documented
- Payload must be < 256KB (IRAM limit)
- PIN fail-safe: incorrect PIN must NEVER lead to Nyx
- Config fail-safe: no config found → show error on screen, don't chainload

### What NOT to Do

- ❌ Do NOT modify any code in `app/vendor/hekate/` — we do not fork Hekate
- ❌ Do NOT use `lv_label` — font rendering doesn't work in standalone payload (use BMP assets)
- ❌ Do NOT hardcode hardware offsets — verify in Hekate source first
- ❌ Do NOT add secrets, API keys, or credentials to the repository

### Testing

- All code changes should be tested on hardware before release
- Document hardware test status in release notes
- If hardware testing is not possible, mark as "⚠️ Hardware tests pending"

## Code of Conduct

Please note that this project follows a [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to uphold its standards.

## License

By contributing, you agree that your contributions will be licensed under GPL-2.0.
