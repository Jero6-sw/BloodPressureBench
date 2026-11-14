/*
 * report.js - rapport de session (cote navigateur).
 * Priorite : jsPDF (PDF natif, fichier jspdf.umd.min.js dans la partition
 * LittleFS). Si jsPDF est absent, REPLI automatique : page imprimable ouverte
 * dans un onglet -> "Imprimer > Enregistrer en PDF" (meme resultat, zero
 * dependance).
 */
async function generateReportPDF() {
  const r = await (await fetch('/api/report')).json();
  var mean = r.displayed.reduce(function (a, b) { return a + b; }, 0) / 5;
  var sd = Math.sqrt(r.displayed.reduce(function (a, b) { return a + Math.pow(b - mean, 2); }, 0) / 4);

  if (window.jspdf && window.jspdf.jsPDF) {
    const doc = new window.jspdf.jsPDF();
    doc.setFontSize(14);
    doc.text('Banc de test - rapport de session', 14, 15);
    doc.setFontSize(10);
    doc.text('Operateur : ' + r.operator + '    Patient : ' + r.patient + '    T : ' + r.tempC + ' C', 14, 26);
    doc.text('Appareil teste : ' + r.deviceSN + ' (firmware ' + r.fwDevice + ')', 14, 32);
    doc.text('Banc v' + r.version, 14, 38);
    doc.text('- Auto-diagnostic -', 14, 48);
    doc.text('Fuite banc seul : ' + r.leakSlope.toFixed(2) + ' mmHg/min (seuil < 1)', 14, 56);
    doc.text('Fuite bras seul : ' + r.leakSlopeArm.toFixed(2) + ' mmHg/min (seuil < 1)', 14, 62);
    doc.text('Volume Boyle : ' + r.boyleV.toFixed(0) + ' mL (fiche bras : consigner)', 14, 68);
    doc.text('Paliers ABP vs aneroide : ' + r.pReads.join(' / ') + ' mmHg', 14, 74);
    doc.text('Zero ABP (air libre) : ' + r.zeroOffset.toFixed(2) + ' mmHg - Gain (Boyle) : ' + r.gainErrPct.toFixed(2) + ' %', 14, 80);
    doc.text('- Mesures (5) -', 14, 92);
    r.displayed.forEach(function (v, i) {
      doc.text('Mesure ' + (i + 1) + ' : ' + v + ' mmHg', 14, 100 + 6 * i);
    });
    doc.text('Moyenne : ' + mean.toFixed(1) + ' mmHg - Ecart-type : ' + sd.toFixed(2), 14, 136);
    doc.text('Indicateur de derive : comparer la moyenne et la dispersion aux sessions precedentes.', 14, 142);
    doc.text('(Alerte : derive monotone > 3 mmHg cumules sur la moyenne des 5 valeurs.)', 14, 148);
    doc.save('rapport-banc-' + Date.now() + '.pdf');
    return;
  }

  // ---- REPLI : jsPDF absent de la flash -> page imprimable ----
  var w = window.open('', '_blank');
  if (!w) { alert('jsPDF absent et popup bloquee : autoriser les popups ou ajouter jspdf.umd.min.js dans data/www/.'); return; }
  var rows = r.displayed.map(function (v, i) { return '<tr><td>Mesure ' + (i + 1) + '</td><td>' + v + ' mmHg</td></tr>'; }).join('');
  w.document.write('<html><head><title>Rapport banc</title></head><body style="font-family:system-ui">' +
    '<h2>Banc de test - rapport de session (banc v' + r.version + ')</h2>' +
    '<p>Operateur : <b>' + r.operator + '</b> - Patient : <b>' + r.patient + '</b> - T : ' + r.tempC + ' C<br>' +
    'Appareil teste : ' + r.deviceSN + ' (firmware ' + r.fwDevice + ')</p>' +
    '<h3>Auto-diagnostic</h3><ul>' +
    '<li>Fuite banc seul : ' + r.leakSlope.toFixed(2) + ' mmHg/min (seuil &lt; 1)</li>' +
    '<li>Fuite bras seul : ' + r.leakSlopeArm.toFixed(2) + ' mmHg/min (seuil &lt; 1)</li>' +
    '<li>Volume Boyle : ' + r.boyleV.toFixed(0) + ' mL</li>' +
    '<li>Paliers ABP vs aneroide : ' + r.pReads.join(' / ') + ' mmHg</li>' +
    '<li>Zero ABP (air libre) : ' + r.zeroOffset.toFixed(2) + ' mmHg - Gain (Boyle) : ' + r.gainErrPct.toFixed(2) + ' %</li>' +
    '</ul><h3>Mesures (5)</h3><table border="1" cellpadding="6" style="border-collapse:collapse">' + rows + '</table>' +
    '<p>Moyenne : <b>' + mean.toFixed(1) + ' mmHg</b> - Ecart-type : ' + sd.toFixed(2) + '</p>' +
    '<p style="color:#777">Indicateur de derive : comparer la moyenne et la dispersion aux sessions precedentes. ' +
    '(Alerte : derive monotone &gt; 3 mmHg cumules sur la moyenne des 5 valeurs.)</p>' +
    '<script>window.onload=function(){setTimeout(function(){window.print()},200)}</script>' +
    '</body></html>');
  w.document.close();
}
