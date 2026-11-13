# Variante : automatisation des robinets (électrovannes 2/2)

Remplace les robinets manuels S1-S4 par des électrovannes pilotées par l'ESP32. Voir `README_FR.md` § 3 pour l'architecture de base.

## Contrainte de sélection — vannes direct-actionnées dès 0 bar

> ⚠️ Correction : la **2V130** est à **diaphragme piloté** (comme la 2W), contrairement à la 2V025 qui est à piston direct. Seules les vannes **direct-actionnées dès 0 bar** conviennent.

Le circuit plafonne à ~300 mmHg (**0,04 MPa**). Les électrovannes pneumatiques **assistées par pression** (séries 2W, 3V, 4V — pression mini 0,02 à 0,15 MPa) **ne s'ouvriraient jamais** à nos pressions : exclues. Exemple : la 3V210-08 (mini 0,15 MPa = 1 125 mmHg) est inutilisable ici.

### Comparatif détaillé des vannes candidates

| Vanne | Principe | Pression mini | Orifice / ports | Prix | Verdict |
|---|---|---|---|---|---|
| **2V025-06** (AirTAC et clones) | poppet **direct** | **0 MPa** ✅ | 2,5 mm / G1/8 | ~10 € | **Retenue** — budget |
| 2V130-15 | **diaphragme piloté** | ~0,05 MPa ❌ | 3 mm / G1/2 | ~12 € | **Exclue** — pilotée, ne s'ouvre pas à nos pressions |
| 2W025-06 | diaphragme | ~0,02 MPa (150 mmHg) ❌ | 2,5 mm / G1/8 | ~8 € | **Exclue** — échoue à basse pression (purge à 0, Boyle) |
| 3V210-08 (3/2) | assisté | 0,15 MPa ❌ | 3 mm / G1/4 | ~15 € | **Exclue** (1 125 mmHg mini) |
| **SMC VT307 / VT307V** | poppet **direct** | **0 MPa** ✅ | 2-3 mm / Rc 1/8 → 1/2 | ~30-50 € | **Retenue** — qualité |

**Détails SMC VT307** :
- `3G1 / 4G1 / 5G1 / 6G1` = taille de port (Rc 1/8, 1/4, 3/8, 1/2) → prendre **3G1** (Rc 1/8) ;
- suffixe `-01 / -02` = **diamètre d'orifice** (petit / gros) → prendre **-01** (Ø 2 mm, largement suffisant pour 2 mL/s) ;
- `VT307V` = variante **spécification vide** (vacuum) à **fuite interne réduite** → recommandée pour les vannes du chemin de mesure (V1, V5) ;
- `VT307W` = V + économie d'énergie (bobine) ; `VT307E` = service continu (100 % ED) ;
- **Porting universel** : la même vanne se câble en NC ou NO selon le branchement → une seule référence en stock pour les 7 rôles.

**Référence retenue (achat effectué, ~10 €/pièce)** : **`SMC VT307V-6G1-01` DC12 V** ×7.
- Port **6G1 = Rc 1/2**, plus gros que le 3G1 nominal → prévoir 7 **réductions Rc 1/2 → 1/8** (ou raccords push-in Ø 8/6 mm + adaptateur vers tuyau 4 mm) ;
- Orifice -01 (Ø 2 mm) : hydraulique identique, aucun impact sur les fronts de pression ;
- Variante **V** (vacuum, fuite interne réduite) sur tout le banc, chemin de mesure inclus — et cette même référence peut servir de vanne de purge (V6).
- Alternative budget non retenue : `2V025-06` ×7.

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
