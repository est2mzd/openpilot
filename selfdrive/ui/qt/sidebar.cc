#include "selfdrive/ui/qt/sidebar.h"

#include <QMouseEvent>

#include "selfdrive/ui/qt/util.h"

// Side-Barの Metric描画用の共通関数
void Sidebar::drawMetric(QPainter &p, const QPair<QString, QString> &label, QColor c, int y) {
  const QRect rect = {30, y, 240, 126};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + 4, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + 4, rect.y() + 4, 100, 118), 18, 18);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setFont(InterFont(35, QFont::DemiBold));
  p.drawText(rect.adjusted(22, 0, 0, 0), Qt::AlignCenter, label.first + "\n" + label.second);
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent), onroad(false), flag_pressed(false), settings_pressed(false) {
  home_img = loadPixmap("../assets/images/button_home.png", home_btn.size());
  flag_img = loadPixmap("../assets/images/button_flag.png", home_btn.size());
  settings_img = loadPixmap("../assets/images/button_settings.png", settings_btn.size(), Qt::IgnoreAspectRatio);

  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);

  pm = std::make_unique<PubMaster>(std::vector<const char*>{"userFlag"});
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  if (onroad && home_btn.contains(event->pos())) {
    flag_pressed = true;
    update();
  } else if (settings_btn.contains(event->pos())) {
    settings_pressed = true;
    update();
  }
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (flag_pressed || settings_pressed) {
    flag_pressed = settings_pressed = false;
    update();
  }
  if (onroad && home_btn.contains(event->pos())) {
    MessageBuilder msg;
    msg.initEvent().initUserFlag();
    pm->send("userFlag", msg);
  } else if (settings_btn.contains(event->pos())) {
    emit openSettings();
  }
}

void Sidebar::offroadTransition(bool offroad) {
  onroad = !offroad;
  update();
}

void Sidebar::updateState(const UIState &s) {
  if (!isVisible()) return;

  auto &sm = *(s.sm);

  networking = networking ? networking : window()->findChild<Networking *>("");
  bool tethering_on = networking && networking->wifi->tethering_on;
  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", tethering_on ? "Hotspot": network_type[deviceState.getNetworkType()]);
  int strength = tethering_on ? 4 : (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = ItemStatus{{tr("CONNECT"), tr("OFFLINE")}, warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, good_color}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  ItemStatus tempStatus = {{tr("TEMP"), tr("HIGH")}, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    tempStatus = {{tr("TEMP"), tr("GOOD")}, good_color};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    tempStatus = {{tr("TEMP"), tr("OK")}, warning_color};
  }
  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {{tr("VEHICLE"), tr("ONLINE")}, good_color};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {{tr("NO"), tr("PANDA")}, danger_color};
  }
  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));
}

// サイドバー描画の本体
void Sidebar::paintEvent(QPaintEvent *event) {
  // QPainter を用いて Sidebar 全体を描画（this は Sidebar ウィジェット）
  QPainter p(this);

  // 線を描かない（輪郭線なし）
  p.setPen(Qt::NoPen);

  // アンチエイリアス（角を滑らかに）を有効にする
  p.setRenderHint(QPainter::Antialiasing);

  // Sidebar 全体の背景を暗灰色で塗りつぶす
  p.fillRect(rect(), QColor(57, 57, 57));

  // ================== 歯車ボタン ================== //
  // 設定ボタンが押されたときは少し薄く表示（押下エフェクト）
  p.setOpacity(settings_pressed ? 0.65 : 1.0);
  // 設定アイコンを指定位置に描画（左上の歯車）
  p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);

  // ホーム（またはフラグ）ボタンが押されたときの不透明度設定
  p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);
  p.drawPixmap(home_btn.x(), home_btn.y(), onroad ? flag_img : home_img);
  p.setOpacity(1.0);

  // ================== 電波強度を示す5個のマル ================== //
  // network
  int x = 58; // 最初の円のX座標
  const QColor gray(0x54, 0x54, 0x54);
  for (int i = 0; i < 5; ++i) {
    // 通信強度（net_strength）より下のインデックスは白、それ以外はグレー
    p.setBrush(i < net_strength ? Qt::white : gray);

    // 小さな円を描画（信号強度アイコンを模倣）
    p.drawEllipse(x, 196, 27, 27);
    x += 37;  // 次の円のX位置に移動
  }

  // ================== 通信種別の文字列（"Wi-Fi"など）を描画 ================== //
  p.setFont(InterFont(35));
  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(58, 247, width() - 100, 50);
  // net_typeの定義 : NETWORK_TYPES in openpilot/selfdrive/ui/layouts/sidebar.py
  p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, net_type);


  // ================== Metrics を描画 ================== //
  // drawMetric(pointer, label, color, pos-y)
  // 温度ステータス（TEMP GOOD など）を描画（上段、緑系）
  drawMetric(p, temp_status.first, temp_status.second, 338);

  // Panda（ECU）との接続状態（NO PANDA など）を描画（中段、赤系）
  drawMetric(p, panda_status.first, panda_status.second, 496);

  // 通信状態（CONNECT OFFLINE など）を描画（下段、黄系）
  drawMetric(p, connect_status.first, connect_status.second, 654);
}
