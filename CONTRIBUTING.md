# Contributing to ThunderOS

Thank you for your interest in contributing to ThunderOS! This guide will help you get started.

## Getting Started

1. **Read the documentation**
   - [README.md](README.md) - Project overview and quick start
   - [ROADMAP.md](ROADMAP.md) - Planned features and versioning
   - [CHANGELOG.md](CHANGELOG.md) - What's already implemented
   - [docs/](docs/) - Full technical documentation (build with `make html`)

2. **Check current status**
   - Look at the README banner for any active code freezes
   - See [FREEZE.md](FREEZE.md) if a freeze is active
   - Review open issues and pull requests

3. **Set up your environment**
   - Follow setup instructions in [docs/source/development.rst](docs/source/development.rst)
   - Install RISC-V toolchain and QEMU
   - Build and test: `make && make qemu`

## How to Contribute

### Reporting Bugs

- Open an issue with clear reproduction steps
- Include QEMU version, toolchain version, and OS
- Attach relevant error messages or kernel output

### Suggesting Features

- Check [ROADMAP.md](ROADMAP.md) first - feature might be planned
- Open an issue describing the feature and its use case
- Wait for maintainer feedback before implementing

### Submitting Code

1. **Choose the right branch**
   - **Active development**: `dev/vX.Y.Z` (check README for current version)
   - **Never merge directly to `main`** - it's protected

2. **Follow coding standards**
   - See [docs/source/development/code_quality.rst](docs/source/development/code_quality.rst)
   - Use `-O0` optimization (for debugging)
   - Run `make` to check for compiler warnings

3. **Write tests**
   - Add tests in `tests/` directory
   - Follow the KUnit-inspired framework in [docs/source/internals/testing_framework.rst](docs/source/internals/testing_framework.rst)

4. **Document your changes**
   - Update relevant `.rst` files in `docs/source/`
   - Add entry to [CHANGELOG.md](CHANGELOG.md) under "Unreleased"
   - Comment your code clearly

5. **Submit a pull request**
   - Target the current `dev/vX.Y.Z` branch
   - Write a clear PR description
   - Reference related issues
   - Wait for review

## AI Usage Policy

ThunderOS uses AI assistance for development. See [AI_USAGE.md](AI_USAGE.md) for guidelines on:
- What AI can/cannot be used for
- How to attribute AI-generated code
- Review requirements for AI contributions

## Code of Conduct

- Be respectful and constructive
- Focus on technical merit
- Help newcomers learn RISC-V and OS development
- Remember: this is an educational project

## Questions?

- Open a GitHub Discussion for general questions
- Use issues for specific bugs or features
- Check existing documentation first

## License

By contributing, you agree that your contributions will be licensed under the same license as ThunderOS. See [LICENSE](LICENSE).
