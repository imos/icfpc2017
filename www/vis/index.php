<?php

require_once(dirname(__FILE__) . '/../library/api.php');

StartPage();

$data = '{"map": {"mines": [1, 5], "rivers": [{"source": 0, "target": 1}, {"source": 1, "target": 2}, {"source": 0, "target": 7}, {"source": 7, "target": 6}, {"source": 6, "target": 5}, {"source": 5, "target": 4}, {"source": 4, "target": 3}, {"source": 3, "target": 2}, {"source": 1, "target": 7}, {"source": 1, "target": 3}, {"source": 7, "target": 5}, {"source": 5, "target": 3}], "sites": [{"id": 0, "x": 0, "y": 0}, {"id": 1, "x": 1, "y": 0}, {"id": 2, "x": 2, "y": 0}, {"id": 3, "x": 2, "y": -1}, {"id": 4, "x": 2, "y": -2}, {"id": 5, "x": 1, "y": -2}, {"id": 6, "x": 0, "y": -2}, {"id": 7, "x": 0, "y": -1}]}, "punter": -1, "punters": 4, "settings": {"futures": true}}
{"claim": {"punter": 0, "source": 1, "target": 3}}
{"claim": {"punter": 1, "source": 3, "target": 5}}
{"claim": {"punter": 2, "source": 0, "target": 1}}
{"claim": {"punter": 3, "source": 5, "target": 6}}
{"claim": {"punter": 0, "source": 1, "target": 2}}
{"claim": {"punter": 1, "source": 3, "target": 4}}
{"claim": {"punter": 2, "source": 0, "target": 7}}
{"claim": {"punter": 3, "source": 1, "target": 7}}
{"claim": {"punter": 0, "source": 2, "target": 3}}
{"claim": {"punter": 1, "source": 4, "target": 5}}
{"claim": {"punter": 2, "source": 5, "target": 7}}
{"claim": {"punter": 3, "source": 6, "target": 7}}';

$battle_id = 1;

if (isset($_GET['battle_id'])) {
  $battle_id = intval($_GET['battle_id']);
}

$battle = Database::SelectRow('SELECT * FROM battle NATURAL JOIN map NATURAL JOIN battle_log WHERE battle_id = {battle_id} ORDER BY battle_log_id DESC LIMIT 1', ['battle_id' => $battle_id]);
if (isset($battle['battle_id'])) {
  foreach (Database::Select('SELECT * FROM punter NATURAL JOIN ai WHERE battle_id = {battle_id} ORDER BY punter_id', ['battle_id' => $battle['battle_id']]) as $punter) {
    $battle['punters'][] = $punter;
  }
}

if (isset($_GET['battle_id'])) {
  $lines = explode("\n", $battle['battle_log_data']);
  $lines = array_map('trim', $lines);
  $data = implode("\n", $lines);
}

$STYLESHEET = '
.emscripten { padding-right: 0; margin-left: auto; margin-right: auto; display: block; }
div.emscripten { text-align: center; }
div.emscripten_border { /*border: 1px solid black;*/ }
/* the canvas *must not* have any border or padding, or mouse coords will be wrong */
canvas.emscripten { border: 0px none; }


.spinner {
  height: 30px;
  width: 30px;
  margin: 0;
  margin-top: 20px;
  margin-left: 20px;
  display: inline-block;
  vertical-align: top;

  -webkit-animation: rotation .8s linear infinite;
  -moz-animation: rotation .8s linear infinite;
  -o-animation: rotation .8s linear infinite;
  animation: rotation 0.8s linear infinite;

  border-left: 5px solid #EE3987;
  border-right: 5px solid #EE3987;
  border-bottom: 5px solid #EE3987;
  border-top: 5px solid #CCCCCC;

  border-radius: 100%;
  background-color: #EEEEEE;
}

@-webkit-keyframes rotation {
  from {-webkit-transform: rotate(0deg);}
  to {-webkit-transform: rotate(360deg);}
}
@-moz-keyframes rotation {
  from {-moz-transform: rotate(0deg);}
  to {-moz-transform: rotate(360deg);}
}
@-o-keyframes rotation {
  from {-o-transform: rotate(0deg);}
  to {-o-transform: rotate(360deg);}
}
@keyframes rotation {
  from {transform: rotate(0deg);}
  to {transform: rotate(360deg);}
}

#status {
  display: inline-block;
  vertical-align: top;
  margin-top: 30px;
  margin-left: 20px;
  font-weight: bold;
  color: rgb(120, 120, 120);
}

#progress {
  height: 20px;
  width: 30px;
}

#controls {
  display: inline-block;
  float: right;
  vertical-align: top;
  margin-top: 30px;
  margin-right: 20px;
}

#output {
  width: 100%;
}
';

?>
    <div class="container">
      <?php ShowBattle($battle); ?>
      <h3>ビジュアライザ</h3>
      <div class="spinner" id='spinner'></div>
      <div class="emscripten" id="status" style="display:none">Downloading...</div>
      <div class="emscripten">
        <progress value="0" max="100" id="progress" hidden=1></progress>
      </div>

      <div class="emscripten_border">
        <canvas class="emscripten" id="canvas" oncontextmenu="event.preventDefault()"></canvas>
      </div>
      <br>
      <textarea class="form-control" id="output" rows="3" readonly></textarea>

      <hr>
      <div class="row">
        <div class="col-sm-6 form-group" id="input_drop" ondragover="event.preventDefault();$(this).css('background-color', '#CCC');" ondragleave="$(this).css('background-color', 'white');" ondrop="inputFileDropped(event);$(this).css('background-color', 'white');" style="padding-bottom:20px;">
          <h2>入力</h2>
          <input id="input_file" type="file" onchange="inputFileChanged(this.files[0]);"><br>
          <textarea class="form-control" id="input" rows="8"><?php echo $data; ?></textarea>
          <br>
          <button class="btn btn-default" onclick="Module.setInput(document.getElementById('input').value)" style="width:100%">更新</button>
        </div>
      	<div class="col-sm-6 form-group">
      	  <h2>使い方</h2>
	      <ul>
	        <li>ファイルはドラッグ＆ドロップでも指定できるはず。</li>
	        <li>コンソール出力を見とくといいです。</li>
	        <li>u/j ・・・ 1 ステップ前後</li>
	        <li>i/k ・・・ 全員 1 ターン前後</li>
	        <li>o/l ・・・ 10% 前後</li>
	        <li>p/; ・・・ 最初・終わり</li>
	        <li>y/h ・・・ 線を細く・太く</li>
	        <li>1/2 ・・・ 表示プレイヤー切り替え</li>
	        <li>a ・・・ アニメーションオンオフ</li>
	      </ul>

          <center>
          <img src="004.png">
          </center>
      </div><!--/row-->

      <script type='text/javascript'>
        var statusElement = document.getElementById('status');
        var progressElement = document.getElementById('progress');
        var spinnerElement = document.getElementById('spinner');

        var Module = {
          preRun: [],
          postRun: [],
          print: (function() {
            var element = document.getElementById('output');
            if (element) element.value = ''; // clear browser cache
            return function(text) {
              if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
              // These replacements are necessary if you render to raw HTML
              //text = text.replace(/&/g, "&amp;");
              //text = text.replace(/</g, "&lt;");
              //text = text.replace(/>/g, "&gt;");
              //text = text.replace('\n', '<br>', 'g');
              console.log(text);
              if (element) {
                element.value += text + "\n";
                element.scrollTop = element.scrollHeight; // focus on bottom
              }
            };
          })(),
          printErr: function(text) {
            if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
            if (0) { // XXX disabled for safety typeof dump == 'function') {
              dump(text + '\n'); // fast, straight to the real console
            } else {
              console.error(text);
            }
          },
          canvas: (function() {
            var canvas = document.getElementById('canvas');

            // As a default initial behavior, pop up an alert when webgl context is lost. To make your
            // application robust, you may want to override this behavior before shipping!
            // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
            canvas.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

            return canvas;
          })(),
          setStatus: function(text) {
            if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
            if (text === Module.setStatus.text) return;
            var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
            var now = Date.now();
            if (m && now - Date.now() < 30) return; // if this is a progress update, skip it if too soon
            if (m) {
              text = m[1];
              progressElement.value = parseInt(m[2])*100;
              progressElement.max = parseInt(m[4])*100;
              progressElement.hidden = false;
              spinnerElement.hidden = false;
            } else {
              progressElement.value = null;
              progressElement.max = null;
              progressElement.hidden = true;
              if (!text) spinnerElement.style.display = 'none';
            }
            statusElement.innerHTML = text;
          },
          totalDependencies: 0,
          monitorRunDependencies: function(left) {
            this.totalDependencies = Math.max(this.totalDependencies, left);
            Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
          }
        };
        Module.setStatus('Downloading...');
        window.onerror = function(event) {
          // TODO: do not warn on ok events like simulating an infinite loop or exitStatus
          Module.setStatus('Exception thrown, see JavaScript console');
          spinnerElement.style.display = 'none';
          Module.setStatus = function(text) {
            if (text) Module.printErr('[post-exception status] ' + text);
          };
        };
      </script>
      <script type='text/javascript'>
        (function() {
          var memoryInitializer = 'icfpc2017.html.mem';
          if (typeof Module['locateFile'] === 'function') {
            memoryInitializer = Module['locateFile'](memoryInitializer);
          } else if (Module['memoryInitializerPrefixURL']) {
            memoryInitializer = Module['memoryInitializerPrefixURL'] + memoryInitializer;
          }
          var xhr = Module['memoryInitializerRequest'] = new XMLHttpRequest();
          xhr.open('GET', memoryInitializer, true);
          xhr.responseType = 'arraybuffer';
          xhr.send(null);
        })();

        var script = document.createElement('script');
        script.src = "icfpc2017.js";
        document.body.appendChild(script);
      </script>
      <script src="//ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js"></script>
      <script src="//maxcdn.bootstrapcdn.com/bootstrap/3.2.0/js/bootstrap.min.js"></script>

      <script>
        function inputFileChanged(f) {
          var r = new FileReader();
          r.onload = function(s) {
            txt = s.target.result;
            $("#input").val(txt);
            Module.setInput(txt);
          };
          r.readAsText(f);
        }

        function inputFileDropped(e) {
          inputFileChanged(e.dataTransfer.files[0]);
          e.preventDefault();
          e.stopPropagation();
        }

        function loadInput(filepath) {
          $.ajax({
              url: filepath,
              success: function(txt) {
                $("#input").val(txt);
                Module.setInput(txt);
              }
            });
        }

        function solutionFileChanged(f) {
          var r = new FileReader();
          r.onload = function(s) {
            txt = s.target.result;
            $("#solution").val(txt);
            Module.setSolution(txt);
          };
          r.readAsText(f);
        }

        function solutionFileDropped(e) {
          solutionFileChanged(e.dataTransfer.files[0]);
          e.preventDefault();
          e.stopPropagation();
        }

        function loadSolution(filepath) {
          $.ajax({
              url: filepath,
              success: function(txt) {
                $("#solution").val(txt);
                Module.setSolution(txt);
              }
            });
        }

        function loadInputAndSolution(input_filepath, solution_filepath) {
          $.ajax({
            url: input_filepath,
            success: function(input_txt) {
              $("#input").val(input_txt);
              $.ajax({
                url: solution_filepath,
                success: function(solution_txt) {
                  $("#solution").val(solution_txt);
                  Module.setInputAndSolution(input_txt, solution_txt);
                }
              });
            }
          });
        }
      </script>
    </div> <!--container-->
    <script>
    $(function(){
      var data = <?php echo json_encode($data); ?>;
      document.getElementById('input').value = data;
      setTimeout(function() { Module.setInput(data); }, 3000);
    });
    </script>
