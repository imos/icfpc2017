<?php

function KeepAlive($interval = 10) {
  global $battle;
  $result = file_get_contents(
      "http://proxy.sx9.jp/api/keep_alive_battle.php?battle_id={$battle['battle_id']}&interval=$interval");
}

$battle = json_decode(file_get_contents(
    'http://proxy.sx9.jp/api/get_battle.php'), TRUE);
if (!isset($battle['battle_id'])) {
  fwrite(STDERR, "No battle.\n");
  file_get_contents('http://proxy.sx9.jp/api/add_random_battle.php');
  exit();
}

$args = [];
foreach ($battle['punter'] as $punter) {
  $args[$punter['punter_id']] = $punter['ai_command'];
}

$master = [
    '/binary/local',
    "--map=/github/map/{$battle['map_key']}.json"];
foreach ($battle['punter'] as $punter) {
  $master[] = $punter['ai_command'];
}

$ninestream = [
    '/binary/ninestream',
    '--communicate',
    '--master=' . implode(' ', array_map('escapeshellarg', $master))];


function Post($url, $data) {
  $data = http_build_query($data);

  $context_options = [
      'http' => [
          'method' => 'POST',
          'header'=>
              "Content-Type: application/x-www-form-urlencoded\r\n" .
              "Content-Length: " . strlen($data) . "\r\n",
          'content' => $data]];

  $context = stream_context_create($context_options);
  $result = file_get_contents($url, false, $context);
}

function GetScores($command) {
  global $battle;
  fwrite(STDERR, "Running command: $command\n");
  KeepAlive(1800);
  exec($command, $output, $return);
  Post("http://proxy.sx9.jp/api/add_battle_log.php?" .
       "battle_id={$battle['battle_id']}",
       ['battle_log_data' => implode("\n", $output)]);
  foreach ($output as $line) {
    $result = json_decode($line, TRUE);
    if (isset($result['scores'])) {
      return $result;
    }
  }
  fwrite(STDERR, 'Score is not found.');
  exit(1);
}

$scores = GetScores(implode(' ', array_map('escapeshellarg', $ninestream)));
foreach ($scores['scores'] as $index => $score) {
  $punter = $battle['punter'][$index];
  $result = file_get_contents(
      "http://proxy.sx9.jp/api/update_punter.php?" .
      "punter_id={$punter['punter_id']}&punter_score={$score['score']}");
}
