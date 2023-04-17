# TODO

- **Backwards speed:** 5% - Rückwärts maximal x%. Niemals mehr Leistung.
- **Siren:** Signalgeber (ggfs. Piep über Motor)
- **Webinterface:** Basisinformationen abrufen und ggfs. Einstellungen anpassen.
- **Batteriewarnung:** Lager Piep. Je 10% Akkuladung abgerundet. Ab 30% Rest.
- **Energy Saver:** 20% - Leistung reduzieren ab X% auf max 50%.
- **Notaus:** Bei plötzlichem Anstieg an Strom bzw. Einbruch der Drehzahl direkt stoppen (Hand im Rotor).
- **Update über WiFi**
- **Gerät muss auch bei Wassereinbruch funktionieren. Es muss aber ein Signal geben, damit man es bemerkt.**
- **Licht Auf Stufe 1 falls Geschwindigkeit über 80% um überlastung vom Akku zu vermeiden. Bei 100% Leistung Licht aus. Danach wieder ursprünglichen Wert.**

# KlickCodes

| Schalter 1 | Schalter 2 | Funktion |
|:----------:|:----------:|:---------|
| halten     | halten     | Motor AN |
| halten     |            | Motor AN |
|            | halten     | Motor AN |
| 1 Klick    | 1 Klick    |          |
|            | 1 Klick    |          |
| 2 Klick    | 2 Klick    | Richtungswechsel |
| 2 Klick    |            | Reaktivierung |
|            | 2 Klick    | Reaktivierung |
| 3 Klick    | 3 Klick    |           |
| 3 Klick    |            | Akkustand |
|            | 3 Klick    | Licht Stufe 1,2,3,AUS |
| 2 Klick    | halten     | Schrittweise langsamer |
| halten     | 2 Klick    | Schrittweise schneller |
