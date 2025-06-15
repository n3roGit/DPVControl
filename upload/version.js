async function fetchVersion() {
    try {
        // Try to get version from local API first (embedded or LittleFS)
        const response = await fetch('/api/version');
        const data = await response.json();
        document.getElementById('version').textContent = data.version;
    } catch (error) {
        console.log('Local version API failed, trying GitHub fallback:', error);
        try {
            // Fallback to GitHub
            const response = await fetch('https://raw.githubusercontent.com/n3roGit/DPVControl/main/data/version.txt');
            const version = await response.text();
            document.getElementById('version').textContent = version.trim();
        } catch (fallbackError) {
            console.error('Both version sources failed:', fallbackError);
            document.getElementById('version').textContent = '2.0.0';
        }
    }
}
fetchVersion(); 