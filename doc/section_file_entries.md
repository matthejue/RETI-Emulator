Assemble mode now requires a same-basename `.section` file next to the `.reti` input. The `.section` JSON must contain unsigned 32-bit integer entries in this layout:

```json
{
  "codesegment_start": 0,
  "datasegment_start": 4572,
  "stack_start": 8000
}
```

`-a/--assemble` writes these three values as the first three encoded 32-bit words in the generated `.bin`, before appending the encoded SRAM words from the loaded `.reti` program.
