const fs = require('fs');
const cssFilePath = './gonggospec/css/asyncapi.min.css';
const addCss = '.aui-root .bg-red-600 {--tw-bg-opacity: 1;background-color: rgba(252, 148, 3, var(--tw-bg-opacity))}';
const addCss2 = '.aui-root .font-thin {font-weight: 400;}';

fs.readFile(cssFilePath, 'utf8', (err, data) => {
    if (err) {
        console.error(err);
        return;
    }

    const modifiedData = data + '\n' + addCss + '\n' + addCss2;

    fs.writeFile(cssFilePath, modifiedData, 'utf8', (err) => {
        if (err) {
            console.error(err);
            return;
        }
    });
});
