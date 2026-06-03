# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/

# STM32
- C source files use Windows line endings (\r\n) and contain UTF-8 French characters (e.g., "é"); edit_file tool fails on these files — use sed via shell_command or write_file instead. Confidence: 0.80
- When using sed or Python scripts to modify C files, text-based function boundary detection (e.g., matching `\n}\n`) frequently fails due to \r\n line endings and leaves orphaned code — always read back the full modified region to verify no leftover statements remain outside function bodies. Confidence: 0.80
- When fixing compilation errors or cleaning up debug code, make minimal targeted edits rather than rewriting entire files from scratch, to avoid breaking existing working functionality (e.g., LoRa, sensor pipelines). Confidence: 0.80
