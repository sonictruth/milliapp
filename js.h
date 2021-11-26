const char* js = "                       \
var el=document.getElementById('data');  \
 html = '';                              \
 data.split('|').forEach( session => {   \
  samples = session.split(',');     \
  samples.forEach( s=> {            \
    html += s + '<br>'              \
    } );});                         \
  el.innerHTML = html;              \
";
