/*
 * InkCorner Watchface — PebbleKit JS
 * AppMessage keys:
 *   0 = FONT_CHOICE  : 0=Regular24  1=Regular28  2=Medium24  3=Medium28
 *   1 = BG_CHOICE    : 0=Cream      1=Black      2=White
 */

function loadCfg() {
  return {
    font: +(localStorage.getItem('ic_font') || '0'),
    bg:   +(localStorage.getItem('ic_bg')   || '0')
  };
}

function saveCfg(c) {
  localStorage.setItem('ic_font', c.font);
  localStorage.setItem('ic_bg',   c.bg);
}

function sendMsg(c) {
  Pebble.sendAppMessage(
    { '0': c.font, '1': c.bg },
    function() { console.log('InkCorner: sent ok'); },
    function(e) { console.log('InkCorner: send failed', JSON.stringify(e)); }
  );
}

function buildConfig(c) {
  function radio(name, opts, sel) {
    return opts.map(function(l, i) {
      return '<label class="opt"><input type="radio" name="' + name +
        '" value="' + i + '"' + (i === sel ? ' checked' : '') +
        '><span>' + l + '</span></label>';
    }).join('');
  }

  var h = '<!DOCTYPE html><html><head>'
    + '<meta charset="utf-8">'
    + '<meta name="viewport" content="width=device-width,initial-scale=1">'
    + '<style>'
    + 'body{margin:0;font:15px/1.6 -apple-system,sans-serif;background:#f2f1ed;color:#1a1a1a;padding:20px 20px 40px}'
    + 'h3{font-size:10px;text-transform:uppercase;letter-spacing:.12em;color:#8a7060;margin:24px 0 8px}'
    + 'h3:first-child{margin-top:0}'
    + '.opt{display:flex;align-items:center;gap:12px;background:rgba(255,255,255,0.6);border-radius:8px;padding:13px;margin:5px 0;cursor:pointer;border:1.5px solid transparent}'
    + '.opt:has(input:checked){border-color:#321c14;background:rgba(255,255,255,0.9)}'
    + '.opt input{accent-color:#321c14;width:18px;height:18px;flex-shrink:0;margin:0}'
    + '.opt span{font-size:14px;color:#1a1a1a}'
    + '.swatch{width:22px;height:22px;border-radius:4px;flex-shrink:0;border:1px solid rgba(0,0,0,0.15)}'
    + '.cream{background:#f2f1ed}.black{background:#0a0a0a}.white{background:#fafafa}'
    + '#s{display:block;width:100%;padding:14px;background:#321c14;color:#f2f1ed;border:none;'
    +    'border-radius:8px;font-size:14px;letter-spacing:.06em;text-transform:uppercase;margin-top:28px;cursor:pointer;box-sizing:border-box}'
    + '</style></head><body>'

    + '<h3>Font</h3>'
    + radio('font', [
        'Regular &middot; 24 — lighter, compact',
        'Regular &middot; 28 — lighter, larger',
        'Medium &middot; 24 — balanced, compact',
        'Medium &middot; 28 — balanced, larger'
      ], c.font)

    + '<h3>Background</h3>'
    + '<label class="opt"><input type="radio" name="bg" value="0"' + (c.bg===0?' checked':'') + '><div class="swatch cream"></div><span>ePaper Cream</span></label>'
    + '<label class="opt"><input type="radio" name="bg" value="1"' + (c.bg===1?' checked':'') + '><div class="swatch black"></div><span>ePaper Black</span></label>'
    + '<label class="opt"><input type="radio" name="bg" value="2"' + (c.bg===2?' checked':'') + '><div class="swatch white"></div><span>ePaper White</span></label>'

    + '<button id="s">Save</button>'
    + '<script>'
    + 'function g(n){var e=document.querySelector("input[name="+n+"]:checked");return e?+e.value:0;}'
    + 'document.getElementById("s").onclick=function(){'
    +   'location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify({'
    +   'font:g("font"),bg:g("bg")}));'
    + '};<\/script></body></html>';

  return 'data:text/html,' + encodeURIComponent(h);
}

Pebble.addEventListener('ready', function() {
  console.log('InkCorner: ready');
  sendMsg(loadCfg());
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(buildConfig(loadCfg()));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || e.response === '' || e.response === 'CANCELLED') return;
  var raw = e.response;
  if (raw.indexOf('#') !== -1) raw = raw.substring(raw.lastIndexOf('#') + 1);
  var c;
  try { c = JSON.parse(decodeURIComponent(raw)); } catch(err) { return; }
  saveCfg(c);
  sendMsg(c);
});
