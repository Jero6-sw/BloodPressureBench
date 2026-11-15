# BloodPressureBench

Banc de test automatisé DIY — suivi de la dérive dans le temps d'un ensemble tensiomètre + brassard grand public.

- 🇫🇷 Documentation complète : [README_FR.md](README_FR.md)
- 🇬🇧 Full documentation (English): [README_EN.md](README_EN.md)
- 💻 Firmware ESP32 : [BloodPressureBench.ino](BloodPressureBench.ino) [src/](src/)
- ⚙️ Variante robinets automatisés (électrovannes 2/2) : [docs/automatisation-electrovannes.md](docs/automatisation-electrovannes.md)

**Principle**: submit the monitor + cuff assembly to a rigorously reproducible mechanical stimulus — an artificial "arm" whose parameters do not age. An identical stimulus must produce an identical response: any variation over time = drift of the assembly under test.

Projet Arduino pour ESP32-WROOM. Ne pas oublier le téléchargement des datas avec l'outil 'Upload LittleFS to ESP32'

