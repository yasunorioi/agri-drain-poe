# agri-drain-poe

M5Stack ATOM PoE Kit + DFRobot SEN0575（転倒ます雨量計の流用）→ MQTT + UECS-CCM の
**排液（drain）計測ノード**。[agri-rain-poe](https://github.com/yasunorioi/agri-rain-poe)
のフォークで、tip カウント × mL/tip → 累積排液量[mL] に意味付けを変えただけ。
[agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core)
ライブラリの上に薄く乗っているだけのスケッチ。

## 用途

灌水の**排水率デューティ制御**の排液側入力。中央（RPI / agriha）が

> 排水率 = 排液mL（このノード） ÷ 灌水mL（[agri-flow-poe](https://github.com/yasunorioi/agri-flow-poe)）

を計算して灌水バルブ（ccm_rp リレー）の duty を増減する。ノード自身は制御しない。

## ハードウェア

- **MCU**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **PoE / Ethernet**: M5Stack ATOM PoE Base (W5500 on SPI)
- **センサー**: DFRobot SEN0575 Gravity 雨量センサー（**DIP スイッチを UART モードに**）
  - 排液をファネルで集めて転倒ますに流し込む
  - M5 G26 (TX) → SEN0575 SDA/RX、M5 G32 (RX) ← SEN0575 SCL/TX、VCC=5V
  - I2C は不安定だった実績があるので UART/Modbus RTU 固定（agri-rain-poe と同じ）
- tip カウンタは SEN0575 基板側が保持するので **ESP32 リブートでカウントは消えない**
  （センサー側の電源断でリセット）

## 設定（NVS 永続化）

`Preferences` ネームスペース `drain-cfg`。Web UI の `/config` から編集:

- **共通**: Node ID, hostname, MQTT host/port/user/pass/topic prefix/interval,
  UECS-CCM enable/interval/room/region/priority
- **排液センサ固有**:
  - `mL per tip` — 1転倒あたりの排液量。0.2mm/tip × 集水ファネル面積で初期値を出し、
    既知量を流して実測校正する（デフォルト 3.6 = OGMS 既定）
  - `Order` — CCM チャネルの order

## 配信

| 出力 | 内容 |
|---|---|
| MQTT `<prefix>` | JSON: `drainage_ml`, `raw_tips`, `ml_per_tip`, `work_min`, `node_id`, `uptime_s` |
| CCM `Drainage.cMC` | 累積排液量 (mL)。※UECS 標準語彙ではない（中央が読めればよい） |

## ビルド / 焼き込み

```bash
pio run -e m5atom-poe -t upload                                        # USB-C
pio run -e m5atom-poe -t upload --upload-port agri-drain-01.local      # OTA
```

## 関連プロジェクト

- [agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core) — 共通ライブラリ
- [agri-rain-poe](https://github.com/yasunorioi/agri-rain-poe) — 雨量（フォーク元）
- [agri-flow-poe](https://github.com/yasunorioi/agri-flow-poe) — 灌水流量（排水率の分母側）
- [agri-env-poe](https://github.com/yasunorioi/agri-env-poe) — 温湿度 + 気圧 + CO₂
- [OGMS](https://github.com/yasunorioi/OGMS) — スタンドアロン制御（排水率 duty の参照実装）
- [ccm_rp2350_relay](https://github.com/yasunorioi/ccm_rp2350_relay) — リレー
