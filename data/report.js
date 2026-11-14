/*
 * report.js - generation du rapport de session en PDF (cote navigateur, via jsPDF).
 * Un ESP32 ne peut pas produire nativement un PDF : la page recupere le JSON
 * /api/report et assemble le document ici, puis le telecharge localement.
 */
async function generateReportPDF() {
  const r = await (await fetch('/api/report')).json();
  const doc = new jspdf.jsPDF();
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
  var mean = r.displayed.reduce(function (a, b) { return a + b; }, 0) / 5;
  var sd = Math.sqrt(r.displayed.reduce(function (a, b) { return a + Math.pow(b - mean, 2); }, 0) / 4);
  doc.text('Moyenne : ' + mean.toFixed(1) + ' mmHg - Ecart-type : ' + sd.toFixed(2), 14, 136);
  doc.text('Indicateur de derive : comparer la moyenne et la dispersion aux sessions precedentes.', 14, 142);
  doc.text('(Alerte : derive monotone > 3 mmHg cumules sur la moyenne des 5 valeurs.)', 14, 148);
  doc.save('rapport-banc-' + Date.now() + '.pdf');
}
