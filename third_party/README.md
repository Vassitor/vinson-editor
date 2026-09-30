# Third-party sources

The dependency trees in this directory are extracted without project-specific
source changes. Generated Python bytecode is excluded from the repository.
Their build integration lives in the top-level `cmake/` folder.

| Dependency | Version | Official archive | SHA-256 |
| --- | --- | --- | --- |
| Scintilla | 5.6.6 | `https://www.scintilla.org/scintilla566.tgz` | `b6b08598c68fac90990d010c1142494d707530602b5320753274d045c2b02189` |

Each dependency's license is retained as `License.txt` in its own directory.

The plain-text editor does not link a lexer library. The previously reserved
Lexilla source tree was removed because no application or build target used it.

