const String js = "                       \
var el=document.getElementById('data');  \
 html = '<table><tr><td>Time</td>  <td>mA</td></tr>';                              \
 data.split('|').forEach( (session, index) => {   \
  html += '<tr><td colspan=2>Session '+index+' </td></tr>';       \
  samples = session.split(',');     \
  samples.forEach( s=> {            \
    s = s.split(':'); \
    if(s.length < 2) return; \
    var time = s[0]; \
    var value = parseInt(s[1]); \
    html += '<tr><td>' + time + '</td><td>' + value + '</td></tr>';              \
    } );});     \
  html += '</table>'; \
  el.innerHTML = html;              \
";
