// Writes <file>.br beside each argument. The .wasm is over CloudFront's 10 MB automatic
// compression limit, so it has to be uploaded already encoded.
import { readFileSync, writeFileSync } from 'node:fs';
import { brotliCompressSync, constants } from 'node:zlib';
for (const file of process.argv.slice(2)) {
    const input = readFileSync(file);
    const output = brotliCompressSync(input, { params: {
        [constants.BROTLI_PARAM_QUALITY]: constants.BROTLI_MAX_QUALITY,
        [constants.BROTLI_PARAM_SIZE_HINT]: input.length } });
    writeFileSync(file + '.br', output);
    console.log(`${file}: ${input.length} -> ${output.length} bytes`);
}
