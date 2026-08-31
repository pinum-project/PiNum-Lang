# Contributing to quil

First of all, thank you for your interest in contributing to **quil**! 🚀 We welcome all contributors who share our passion for building a lightweight, efficient, and clean programming language.

To ensure the codebase remains maintainable and high-quality, we have established a few rules and guidelines that all contributors must follow.

---

## 🌿 Branching Policy

All contributions should be directed toward the **`development`** branch. 

- **Do not** submit Pull Requests (PRs) directly to `main`.
- Create a feature branch from `development` for your changes (e.g., `git checkout -b feature/awesome-new-feature`).
- Once your work is ready and tested, submit a PR to merge into `development`.

---

## 🛠 Standards & Guidelines

### 🎨 Code Formatting
We value clean, consistent code. All C source and header files **must** comply with the project's `.clang-format` configuration.

Before committing your changes, please run:
```bash
clang-format -i src/*.c include/*.h
```
Any PR with inconsistent formatting will be requested to be updated.

### 🧠 Memory Safety (Valgrind)
quil is written in C, which means memory management is critical. We have a zero-tolerance policy for memory leaks and invalid accesses.

You **must** use [Valgrind](https://valgrind.org/) to verify your changes. Before submitting a PR, ensure that running the compiler on sample programs produces no errors:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/quil example/helloworld.quil
# also works with .qil files: ./bin/quil example/helloworld.qil
```

### 🔢 Versioning
If you are submitting a **patch** (e.g., a bug fix), you are encouraged to increment the **patch** version in the `VERSION` file and update the version-related macros in `include/version.h`.

- **Do not** modify the major or minor version numbers; these are handled by the maintainers.

---

## 📝 Git Commit Guidelines

We believe that a clear git history is just as important as the code itself. Your commit messages should be descriptive and follow a professional format.

**A good commit message should:**
1.  **Be concise** in the subject line (under 50 characters).
2.  **Use the imperative mood** ("Add feature" instead of "Added feature").
3.  **Provide context** in the body if the change is complex.

**Example:**
```text
feat: add support for ternary operators in parser

- Implemented tokenizer support for '?' and ':'
- Updated AST nodes to include TernaryExpression
- Added parsing logic in parser.c
```

---

## 🧪 Testing

Quality is non-negotiable. Your code should pass all standard tests located in the `test/` directory.

Currently, you should verify your changes by running the `quil` binary against the test files:
```bash
./bin/quil test/lexer-tests/test.quil
# or ./bin/quil test/lexer-tests/test.qil
```
Ensure that the output matches expectations and that no regressions are introduced in existing tests (lexer, parser, etc.).

---

## 🚀 Pull Request Process

1.  **Fork** the repository and create your branch from `development`.
2.  **Implement** your changes, keeping them focused and surgical.
3.  **Format** your code using `clang-format`.
4.  **Verify** memory safety with `Valgrind`.
5.  **Test** your changes against existing `.quil`/`.qil` files in `test/` and `example/`.
6.  **Submit** a Pull Request to the `development` branch with a clear description of what you've done and why.

---

Thank you for helping make quil better! Happy coding! 💻✨
