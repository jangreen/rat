# CAT Infer

## Requirements

### Parsing CAT files: Antlr

Antlr tool must be placed at /usr/local/lib/antlr-4.10.1-complete.jar
(Antlr runtime will be installed and linked by cmake)

```
cd /usr/local/lib
curl -O <https://www.antlr.org/download/antlr-4.10.1-complete.jar>
```

### Rendering proofs: Graphviz

Install Graphviz. For Mac:

`$ brew install graphviz`

-> to enable dot live preview of proofs: open dot preview(extension) on right side together with proof.dot file (configure debounce rendering in settings)

### Linting

This project uses cpplint for linting. the .clang-format file describes lint config. In VS Code you can use 'cpplint' extension (mine) for automatic linting and C/C++ extension (Microsoft) for automatic formatting (enabled in settings.json).
`$ brew install cpplint`
