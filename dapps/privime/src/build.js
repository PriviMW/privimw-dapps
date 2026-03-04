const fs = require('fs');
const path = require('path');

const srcDir = __dirname;
const outDir = path.join(srcDir, '..', 'ui');

// Parse CLI flags
const args = process.argv.slice(2);
const cidArg = args.find(a => a.startsWith('--cid='));
const defaultCid = '97db059a347227d2d71fd0cb7fb5d993343ab1540d2eaf40fe48d131b611635f';
const cid = cidArg ? cidArg.split('=')[1] : defaultCid;

const outDirArg = args.find(a => a.startsWith('--outdir='));
const finalOutDir = outDirArg ? outDirArg.split('=')[1] : outDir;

const useBundle = args.includes('--bundle');

async function build() {
    let jsCode;

    if (useBundle) {
        // Modular build: esbuild bundles ES modules into IIFE
        const esbuild = require('esbuild');
        const result = await esbuild.build({
            entryPoints: [path.join(srcDir, 'js', 'index.js')],
            bundle: true,
            format: 'iife',
            write: false,
            minify: false,
            target: ['es2015'],
        });
        jsCode = result.outputFiles[0].text;
    } else {
        // Direct mode: read JS file as-is (no bundling, preserves globals)
        jsCode = fs.readFileSync(path.join(srcDir, 'js', 'index.js'), 'utf8');
    }

    // Inject CID (handle both single and double quotes — esbuild may convert)
    jsCode = jsCode.replace(/\{\{CID\}\}/g, cid);

    // Read CSS and template (trimEnd to avoid extra blank lines at boundaries)
    const css = fs.readFileSync(path.join(srcDir, 'styles', 'index.css'), 'utf8').trimEnd();
    jsCode = jsCode.trimEnd();
    const template = fs.readFileSync(path.join(srcDir, 'template.html'), 'utf8');

    // Assemble final HTML
    const html = template
        .replace('/* {{CSS}} */', css)
        .replace('// {{JS}}', jsCode);

    // Ensure output directory exists
    fs.mkdirSync(finalOutDir, { recursive: true });
    const outPath = path.join(finalOutDir, 'index.html');
    fs.writeFileSync(outPath, html, 'utf8');

    const lines = html.split('\n').length;
    console.log('Built ' + outPath + ' (' + lines + ' lines, ' + html.length + ' bytes)');
}

build().catch(e => { console.error(e); process.exit(1); });
