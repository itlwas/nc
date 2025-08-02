# Yoc

> **Yocto** - the smallest SI prefix (10⁻²⁴). **Yoc** - the smallest, fastest terminal text editor.

[![Build Status](https://github.com/itlwas/yoc-editor/workflows/nightly/badge.svg)](https://github.com/itlwas/yoc-editor/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/itlwas/yoc-editor/releases)

A lightning-fast, ultra-minimal terminal text editor written in pure C. Born from the philosophy that less is more, Yoc delivers maximum performance with minimal resource consumption.

## ✨ Features

- **⚡ Lightning Fast** - Zero startup delay, instant response
- **🪶 Featherweight** - Only ~50KB binary, minimal memory footprint
- **🌍 Cross-Platform** - Native support for Windows, macOS, and Linux
- **🔤 Full UTF-8** - Complete Unicode support with proper multi-byte handling
- **🎯 Intuitive** - Familiar key bindings, zero learning curve
- **💾 Zero Dependencies** - Just a C compiler, nothing else needed
- **🔧 Self-Contained** - Single binary, no installation required
- **📊 Smart Rendering** - Optimized screen updates with minimal redraws
- **🔄 Efficient Buffer** - Inline storage for small strings, dynamic allocation for large ones

## 🚀 Philosophy

Yoc embodies the principle of **maximum efficiency through minimalism**. Every line of code serves a purpose, every feature is essential. No bloat, no complexity - just pure, fast text editing.

> *"The best code is no code at all"* - but when you need a text editor, Yoc is there.

## 📦 Installation
```bash
# Clone and build
git clone https://github.com/itlwas/yoc-editor.git
cd yoc-editor
make release
```

**Requirements**: C compiler (GCC, Clang, or MSVC) and Make.

## 🎮 Usage

```bash
./yoc              # Start with empty buffer
./yoc filename.txt # Open existing file
```

## 🔧 Technical Details

- **Language**: Pure C (C17 standard)
- **Size**: ~2,500 lines of code
- **Memory**: Optimized with inline buffers (16 bytes) for small strings
- **Performance**: Zero-copy operations where possible
- **Compatibility**: ANSI terminals, Windows Console, Unix terminals

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🤝 About

Yoc is a personal project by [@itlwas](https://github.com/itlwas). The name comes from "Yocto" - the smallest SI prefix, representing the minimal approach to text editing.

### Inspiration
Originally based on [femto](https://github.com/wadiim/femto), Yoc has grown into its own unique editor with significant improvements and optimizations.

---

**Built with ❤️ and pure C**
