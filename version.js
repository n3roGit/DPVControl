async function fetchVersion() {
    const response = await fetch('https://raw.githubusercontent.com/n3roGit/DPVControl/main/DPVController/version.txt');
    const version = await response.text();
    document.getElementById('version').textContent = version;
}
fetchVersion(); 