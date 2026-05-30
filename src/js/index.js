Pebble.addEventListener('ready', function() {
  console.log('InkCorner PebbleKit JS ready');
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('https://your-url/index.html');
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;

  var cfg;
  try {
    cfg = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('InkCorner: failed to parse config: ' + err);
    return;
  }

  Pebble.sendAppMessage(
    {
      '0': parseInt(cfg.FONT_CHOICE),
      '1': parseInt(cfg.BG_CHOICE)
    },
    function() { console.log('InkCorner: settings sent OK'); },
    function(e) { console.log('InkCorner: settings send failed: ' + JSON.stringify(e)); }
  );
});
