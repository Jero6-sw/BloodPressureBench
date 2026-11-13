# Variante : automatisation des robinets (électrovannes 2/2)

Remplace les robinets manuels S1-S4 par des électrovannes pilotées par l'ESP32. Voir `README_FR.md` § 3 pour l'architecture de base.

## Contrainte de sélection — vannes direct-actionnées dès 0 bar

Le circuit plafonne à ~300 mmHg (**0,04 MPa**). Les électrovannes pneumatiques **assistées par pression** (séries 2W, 3V, 4V — pression mini 0,02 à 0,15 MPa) **ne s'ouvriraient jamais** à nos pressions : exclues. Exemple : la 3V210-08 (mini 0,15 MPa = 1 125 mmHg) est inutilisable ici.

**Séries validées** (poppet direct, 0 MPa → 0,8 MPa, ~10-15 €) :
- `2V025-06` / `2V130-15` (orifice 2,5 / 3 mm, bobine 12 V DC)
- `SMC VT307-5G-01` (déjà en nomenclature pour la purge)

## Table de vannes et matrice de configurations

| Vanne | Type | Rôle | Coupure de courant |
|---|---|---|---|
| V0 | NO | série sur l'alimentation seringue (fermée = S1 45°) | ouverte — sans risque (Schrader scelle le bras) |
| V1 | NO | seringue ↔ nœud M (PULSATION) | ouverte — zéro watt pendant la mesure |
| V2 | NC | seringue ↔ branche clapets (POMPE) | fermée ✅ |
| V3 | NC | manomètre ↔ nœud M (S2) | fermée ✅ |
| V4 | NC | bras ↔ banc (S3) | fermée ✅ (état de sécurité) |
| V5 | NO | ligne CV2 ↔ M (S4) | fermée ✅ |
| V6 | NC | purge atmosphère | fermée ✅ |

| Configuration | V0 | V1 | V2 | V3 | V4 | V5 | V6 |
|---|---|---|---|---|---|---|---|
| Gonflage (pompe) | o | f | o | f | o | o | f |
| Pulsation / mesure | o | o | f | f | o | f | f |
| Test seringue ↔ capteur (banc seul) | o | o | f | f | f | f | f |
| Étanchéité bras seul | f | f | f | f | o | f | f |
| Lectures croisées | — | — | — | o | o | — | — |
| Boyle (seringue cal. sur S2) | f | f | f | o | o | f | f |
| Purge / dégonflage | o | f | f | f | o | f | o |
| Stockage | f | f | f | f | f | f | f |

(o = ouverte, f = fermée, — = indifférente)

## Points de vigilance

1. **V1 est en série dans le chemin oscillant** : ouverte statiquement, restriction et volume mort négligeables (pris dans le V_boyle), mais son siège est une surface de fuite potentielle du circuit utile → auto-test d'isolement à chaque session.
2. **Auto-test par vanne** (firmware : `valveLeakTest()`) : segment pressurisé, vanne fermée, pente < 1 mmHg/min — une fuite de siège est détectée avant de fausser une session.
3. **Exigence 10 ans** : vannes de commodité — mécanique surdimensionnée, joint NBR et bobine sans garantie : acheter **3-4 vannes d'avance** (~40 €).
4. **Pilotage** : bobines 12 V DC, ~5 W — un **ULN2803** (8 canons Darlington) pilote les 7 vannes plus proprement que 7 TIP120.
5. **Raccords** G1/8-G1/4 → adaptateurs Luer / tuyau 4 mm.

## Bilan

Coût équivalent aux robinets manuels inox (~60-90 €), pour : plus aucun geste pneumatique manuel, matrice de configurations exécutée et journalisée par le firmware, procédures entièrement guidées, et état de stockage de sécurité automatique à la coupure de courant.
