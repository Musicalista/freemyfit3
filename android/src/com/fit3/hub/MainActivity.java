package com.fit3.hub;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** One scrolling screen made of cards: a status header, the proxy, the watch, the AI settings and a log. Built in code (no XML resources). */
public final class MainActivity extends Activity {
    private TextView statusText, statusSub, addresses, watchState, logView, brightLabel, groqState;
    private TextView toggle, romState;
    private View statusDot;
    private EditText groqKey, groqModel, filePath;
    private Spinner devices;
    private LinearLayout langRow, aiBody, advBody;
    private final List<BluetoothDevice> bonded = new ArrayList<>();
    private final Handler ui = new Handler(Looper.getMainLooper());
    private static final String[] LANG_NAMES = {"Português", "English", "English US", "Português PT"};
    private static final int[] LANG_IDS = {68, 30, 13, 52};
    private static final int[] BRIGHT = {20, 40, 60, 80, 100};
    private int selLang = 0;
    private boolean dark;
    private int cBg, cCard, cText, cMuted, cAccent, cAccentText, cOk, cBad, cChip, cLog;

    private int dp(int v) { return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()); }
    private GradientDrawable shape(int color, int radiusDp) { GradientDrawable g = new GradientDrawable(); g.setColor(color); g.setCornerRadius(dp(radiusDp)); return g; }
    private TextView label(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(this); t.setText(s); t.setTextSize(sp); t.setTextColor(color); if (bold) t.setTypeface(Typeface.DEFAULT_BOLD); return t;
    }
    private LinearLayout.LayoutParams lp(int w, int h, int l, int t, int r, int b) { LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(w, h); p.setMargins(dp(l), dp(t), dp(r), dp(b)); return p; }
    private LinearLayout card(LinearLayout col, String title, String sub) {
        LinearLayout c = new LinearLayout(this); c.setOrientation(LinearLayout.VERTICAL); c.setPadding(dp(16), dp(14), dp(16), dp(16)); c.setBackground(shape(cCard, 20));
        c.addView(label(title, 17, cText, true));
        if (sub != null) { TextView s = label(sub, 13, cMuted, false); s.setPadding(0, dp(2), 0, dp(6)); c.addView(s); }
        col.addView(c, lp(-1, -2, 0, 0, 0, 12)); return c;
    }
    /** primary = filled accent button, otherwise a tonal one */
    private TextView button(String s, boolean primary, View.OnClickListener l) {
        TextView b = label(s, 14, primary ? cAccentText : cText, true); b.setGravity(Gravity.CENTER); b.setPadding(dp(12), dp(11), dp(12), dp(11));
        b.setBackground(shape(primary ? cAccent : cChip, 14)); b.setClickable(true); b.setOnClickListener(l); return b;
    }
    private LinearLayout row(View... v) {
        LinearLayout r = new LinearLayout(this); r.setOrientation(LinearLayout.HORIZONTAL);
        for (int i = 0; i < v.length; i++) r.addView(v[i], lp(0, -2, i == 0 ? 0 : 6, 0, 0, 0)); for (int i = 0; i < v.length; i++) ((LinearLayout.LayoutParams) v[i].getLayoutParams()).weight = 1f;
        r.setPadding(0, dp(6), 0, 0); return r;
    }
    private EditText field(String hint, String value, boolean secret) {
        EditText e = new EditText(this); e.setHint(hint); e.setText(value); e.setTextSize(14); e.setTextColor(cText); e.setHintTextColor(cMuted); e.setSingleLine(true);
        e.setBackground(shape(cChip, 12)); e.setPadding(dp(12), dp(10), dp(12), dp(10)); if (secret) e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD); return e;
    }
    private View.OnClickListener tap(final Runnable r) { return new View.OnClickListener() { public void onClick(View v) { r.run(); } }; }

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        dark = (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        setTheme(dark ? android.R.style.Theme_Material_NoActionBar : android.R.style.Theme_Material_Light_NoActionBar);
        cBg = dark ? 0xFF11131A : 0xFFF2F3F8; cCard = dark ? 0xFF1C1F2A : 0xFFFFFFFF; cText = dark ? 0xFFEDEFF7 : 0xFF1A1D29; cMuted = dark ? 0xFF9AA0B5 : 0xFF687086;
        cAccent = dark ? 0xFF8FA4FF : 0xFF4257D8; cAccentText = dark ? 0xFF0E1230 : 0xFFFFFFFF; cOk = 0xFF2BB673; cBad = 0xFFE5534B; cChip = dark ? 0xFF2A2E3D : 0xFFE9EBF4; cLog = dark ? 0xFF0B0D13 : 0xFFEEF0F6;
        Window w = getWindow(); if (Build.VERSION.SDK_INT >= 21) { w.setStatusBarColor(cBg); w.setNavigationBarColor(cBg); }
        if (Build.VERSION.SDK_INT >= 23 && !dark) w.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
        ScrollView sv = new ScrollView(this); sv.setBackgroundColor(cBg); sv.setFillViewport(true);
        LinearLayout col = new LinearLayout(this); col.setOrientation(LinearLayout.VERTICAL); col.setPadding(dp(14), dp(14), dp(14), dp(28)); sv.addView(col);
        setContentView(sv);

        // ---- header: app name + live status
        LinearLayout head = new LinearLayout(this); head.setOrientation(LinearLayout.VERTICAL); head.setPadding(dp(20), dp(18), dp(20), dp(18));
        GradientDrawable hg = new GradientDrawable(GradientDrawable.Orientation.TL_BR, dark ? new int[] {0xFF2B3470, 0xFF4B2F73} : new int[] {0xFF4257D8, 0xFF7B4FD6}); hg.setCornerRadius(dp(24)); head.setBackground(hg);
        head.addView(label("Fit3 Hub", 24, 0xFFFFFFFF, true));
        head.addView(label(Hub.t("Galaxy Fit3 · proxy de internet e ferramentas", "Galaxy Fit3 · internet proxy and tools"), 13, 0xCCFFFFFF, false));
        LinearLayout st = new LinearLayout(this); st.setGravity(Gravity.CENTER_VERTICAL); st.setPadding(0, dp(14), 0, 0);
        statusDot = new View(this); st.addView(statusDot, lp(dp(12), dp(12), 0, 0, 10, 0));
        LinearLayout stc = new LinearLayout(this); stc.setOrientation(LinearLayout.VERTICAL);
        statusText = label("", 17, 0xFFFFFFFF, true); statusSub = label("", 13, 0xCCFFFFFF, false); stc.addView(statusText); stc.addView(statusSub); st.addView(stc);
        head.addView(st); col.addView(head, lp(-1, -2, 0, 0, 0, 12));

        // ---- proxy
        LinearLayout proxy = card(col, Hub.t("Proxy do relógio", "Watch proxy"), Hub.t("O relógio usa a ancoragem Bluetooth deste celular para navegar.", "The watch uses this phone's Bluetooth tethering to browse."));
        toggle = button("", true, tap(new Runnable() { public void run() { toggleProxy(); } })); toggle.setTextSize(16); toggle.setPadding(dp(12), dp(15), dp(12), dp(15)); proxy.addView(toggle);
        addresses = label("", 12, cMuted, false); addresses.setPadding(0, dp(10), 0, 0); proxy.addView(addresses);
        proxy.addView(label(Hub.t("Como usar", "How to use"), 14, cText, true), lp(-1, -2, 0, 12, 0, 2));
        proxy.addView(label(Hub.t("1. Ligue o proxy acima.\n2. Ative a ancoragem Bluetooth.\n3. No relógio: Apps extras > Internet.", "1. Start the proxy above.\n2. Turn on Bluetooth tethering.\n3. On the watch: Extra apps > Internet."), 13, cMuted, false));
        proxy.addView(row(button(Hub.t("Abrir ancoragem", "Open tethering"), false, tap(new Runnable() { public void run() { openTethering(); } })),
                          button(Hub.t("Ajustes de Bluetooth", "Bluetooth settings"), false, tap(new Runnable() { public void run() { startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)); } }))));

        // ---- watch
        LinearLayout watch = card(col, Hub.t("Relógio", "Watch"), Hub.t("Controle direto por Bluetooth (sem gravar firmware).", "Direct control over Bluetooth (never flashes firmware)."));
        devices = new Spinner(this); devices.setBackground(shape(cChip, 12)); devices.setPadding(dp(8), dp(6), dp(8), dp(6)); watch.addView(devices, lp(-1, dp(48), 0, 0, 0, 0));
        watchState = label("", 13, cMuted, false); watchState.setPadding(0, dp(8), 0, 0); watch.addView(watchState);
        watch.addView(row(button(Hub.t("Conectar", "Connect"), true, tap(new Runnable() { public void run() { connectWatch(); } })),
                          button(Hub.t("Desconectar", "Disconnect"), false, tap(new Runnable() { public void run() { Hub.watch.close(); refresh(); } }))));
        watch.addView(row(button(Hub.t("Informações", "Info"), false, tap(new Runnable() { public void run() { info(); } })),
                          button(Hub.t("Notificar", "Notify"), false, tap(new Runnable() { public void run() { at("NOTICE=-a=Fit3 Hub: " + Hub.t("teste", "test")); } })),
                          button(Hub.t("Vibrar", "Vibrate"), false, tap(new Runnable() { public void run() { at("MOTOR_VIB=on"); } }))));
        brightLabel = label("", 13, cText, true); brightLabel.setPadding(0, dp(14), 0, 0); watch.addView(brightLabel);
        SeekBar sb = new SeekBar(this); sb.setMax(4); sb.setProgress(2); brightLabel.setText(Hub.t("Brilho: ", "Brightness: ") + "60%");
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean user) { brightLabel.setText(Hub.t("Brilho: ", "Brightness: ") + BRIGHT[p] + "%"); }
            public void onStartTrackingTouch(SeekBar s) {}
            public void onStopTrackingTouch(SeekBar s) { at("SCREENBRIGHT=" + BRIGHT[s.getProgress()]); } });
        watch.addView(sb);
        watch.addView(label(Hub.t("Idioma do relógio", "Watch language"), 13, cText, true), lp(-1, -2, 0, 12, 0, 4));
        langRow = new LinearLayout(this); langRow.setOrientation(LinearLayout.HORIZONTAL); watch.addView(langRow); buildLangChips();

        // ---- Game Boy ROM upload (the same transfer the PC bridge page does, straight from the phone)
        LinearLayout gb = card(col, "Game Boy", Hub.t("Envia um jogo (.gb ou .gbc) para o relógio, em /user/gb.gb. Use um jogo seu.", "Sends a game (.gb or .gbc) to the watch as /user/gb.gb. Bring your own game."));
        romState = label("", 13, cMuted, false); gb.addView(romState);
        gb.addView(button(Hub.t("Escolher jogo e enviar", "Pick a game and send"), true, tap(new Runnable() { public void run() { pickRom(); } })), lp(-1, -2, 0, 8, 0, 0));

        // ---- AI (collapsible)
        LinearLayout ai = card(col, Hub.t("IA (Groq)", "AI (Groq)"), null);
        groqState = label("", 13, cMuted, false); ai.addView(groqState);
        aiBody = new LinearLayout(this); aiBody.setOrientation(LinearLayout.VERTICAL); aiBody.setVisibility(View.GONE);
        groqKey = field(Hub.t("Chave da API (gsk_...)", "API key (gsk_...)"), Hub.groqKey(this), true); aiBody.addView(groqKey, lp(-1, -2, 0, 8, 0, 6));
        groqModel = field(Hub.t("Modelo", "Model"), Hub.groqModel(this), false); aiBody.addView(groqModel);
        aiBody.addView(row(button(Hub.t("Colar", "Paste"), false, tap(new Runnable() { public void run() { pasteKey(); } })),
                           button(Hub.t("Salvar", "Save"), true, tap(new Runnable() { public void run() { saveGroq(); } })),
                           button(Hub.t("Testar", "Test"), false, tap(new Runnable() { public void run() { testAi(); } }))));
        ai.addView(aiBody);
        ai.addView(button(Hub.t("Configurar", "Configure"), false, tap(new Runnable() { public void run() { aiBody.setVisibility(aiBody.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE); } })), lp(-1, -2, 0, 8, 0, 0));

        // ---- advanced (collapsible)
        LinearLayout adv = card(col, Hub.t("Avançado", "Advanced"), null);
        advBody = new LinearLayout(this); advBody.setOrientation(LinearLayout.VERTICAL); advBody.setVisibility(View.GONE);
        filePath = field("/user/...", "/user/notice_reply_data.txt", false); advBody.addView(filePath, lp(-1, -2, 0, 4, 0, 0));
        advBody.addView(row(button(Hub.t("Ler arquivo", "Read file"), false, tap(new Runnable() { public void run() { readFile(); } })),
                            button(Hub.t("Parar vibração", "Stop vibration"), false, tap(new Runnable() { public void run() { at("MOTOR_VIB=off"); } })),
                            button(Hub.t("Reiniciar", "Reboot"), false, tap(new Runnable() { public void run() { confirmReboot(); } }))));
        adv.addView(advBody);
        adv.addView(button(Hub.t("Mostrar", "Show"), false, tap(new Runnable() { public void run() { advBody.setVisibility(advBody.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE); } })), lp(-1, -2, 0, 4, 0, 0));

        // ---- log
        LinearLayout log = card(col, "Log", null);
        logView = label("", 11, cMuted, false); logView.setTypeface(Typeface.MONOSPACE); logView.setTextIsSelectable(true); logView.setBackground(shape(cLog, 12)); logView.setPadding(dp(10), dp(8), dp(10), dp(8));
        log.addView(logView);
        log.addView(button(Hub.t("Copiar log", "Copy log"), false, tap(new Runnable() { public void run() {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE); if (cm != null) { cm.setPrimaryClip(ClipData.newPlainText("log", Hub.logText())); Toast.makeText(MainActivity.this, Hub.t("Log copiado", "Log copied"), Toast.LENGTH_SHORT).show(); } } })), lp(-1, -2, 0, 8, 0, 0));

        Hub.listener = new Hub.Listener() { public void changed() { ui.post(new Runnable() { public void run() { refresh(); } }); } };
        askPermissions(); loadDevices(); refresh();
        ui.postDelayed(new Runnable() { public void run() { refresh(); ui.postDelayed(this, 2000); } }, 2000);
    }

    @Override protected void onDestroy() { Hub.listener = null; super.onDestroy(); }

    private void buildLangChips() {
        langRow.removeAllViews();
        for (int i = 0; i < LANG_NAMES.length; i++) {
            final int idx = i; TextView c = label(LANG_NAMES[i], 12, i == selLang ? cAccentText : cText, true); c.setGravity(Gravity.CENTER); c.setPadding(dp(6), dp(9), dp(6), dp(9));
            c.setBackground(shape(i == selLang ? cAccent : cChip, 12)); c.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { selLang = idx; buildLangChips(); at("LANGUAGE=" + LANG_IDS[idx]); } });
            langRow.addView(c, lp(0, -2, i == 0 ? 0 : 6, 0, 0, 0)); ((LinearLayout.LayoutParams) c.getLayoutParams()).weight = 1f;
        }
    }

    private static final int REQ_ROM = 21;
    private void pickRom() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT); i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("*/*");
        try { startActivityForResult(i, REQ_ROM); } catch (Exception e) { Hub.log(Hub.t("Não consegui abrir o seletor de arquivos", "Could not open the file picker")); }
    }
    @Override protected void onActivityResult(int req, int res, Intent data) {
        if (req != REQ_ROM || res != RESULT_OK || data == null || data.getData() == null) return;
        final android.net.Uri uri = data.getData();
        new Thread(new Runnable() { public void run() { sendRom(uri); } }).start();
    }
    private void romMsg(final String m) { ui.post(new Runnable() { public void run() { romState.setText(m); } }); }
    private void sendRom(android.net.Uri uri) {
        try {
            java.io.InputStream in = getContentResolver().openInputStream(uri); java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream(); byte[] buf = new byte[16384]; int n;
            while ((n = in.read(buf)) > 0) { bo.write(buf, 0, n); if (bo.size() > 4 * 1024 * 1024) throw new java.io.IOException(Hub.t("arquivo maior que 4 MB", "file larger than 4 MB")); } in.close();
            byte[] rom = bo.toByteArray(); if (rom.length < 0x150) throw new java.io.IOException(Hub.t("não parece uma ROM de Game Boy", "does not look like a Game Boy ROM"));
            if (!Hub.watch.isOpen()) {
                final int pos = devices.getSelectedItemPosition(); if (pos < 0 || pos >= bonded.size()) throw new java.io.IOException(Hub.t("escolha o relógio em Relógio e toque em Conectar", "pick the watch under Watch and tap Connect"));
                romMsg(Hub.t("Conectando ao relógio...", "Connecting to the watch...")); Hub.watch.open(bonded.get(pos)); Hub.saveWatch(this, bonded.get(pos).getAddress());
            }
            Hub.log(Hub.t("Enviando ROM: ", "Sending ROM: ") + rom.length + " bytes");
            Hub.watch.writeFile("/user/gb.gb", rom, new WatchLink.Progress() { public void percent(int p) { romMsg(Hub.t("Enviando... ", "Sending... ") + p + "%"); } });
            romMsg(Hub.t("Pronto. No relógio: Apps extras > GameBoy.", "Done. On the watch: Extra apps > GameBoy.")); Hub.log(Hub.t("ROM enviada", "ROM sent"));
        } catch (Exception e) { romMsg(Hub.t("Falha: ", "Failed: ") + e.getMessage()); Hub.log("ROM: " + e.getMessage()); }
    }

    private void askPermissions() {
        List<String> need = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) need.add(Manifest.permission.BLUETOOTH_CONNECT);
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) need.add(Manifest.permission.POST_NOTIFICATIONS);
        if (!need.isEmpty()) requestPermissions(need.toArray(new String[0]), 7);
    }
    @Override public void onRequestPermissionsResult(int code, String[] p, int[] r) { loadDevices(); }

    private void loadDevices() {
        bonded.clear(); List<String> names = new ArrayList<>();
        try {
            BluetoothAdapter ad = BluetoothAdapter.getDefaultAdapter();
            if (ad != null) { Set<BluetoothDevice> set = ad.getBondedDevices(); if (set != null) bonded.addAll(set); }
        } catch (SecurityException e) { Hub.log(Hub.t("Sem permissão de Bluetooth", "No Bluetooth permission")); }
        int pick = 0, i = 0;
        for (BluetoothDevice d : bonded) {
            String n = d.getName() == null ? d.getAddress() : d.getName(); names.add(n + "  (" + d.getAddress() + ")");
            if (d.getAddress().equals(Hub.lastWatch(this)) || (pick == 0 && (n.contains("Fit3") || n.contains("Galaxy Fit")))) pick = i; i++;
        }
        if (names.isEmpty()) names.add(Hub.t("(nenhum dispositivo pareado)", "(no paired devices)"));
        devices.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, names)); devices.setSelection(pick);
    }

    private void refresh() {
        SecureServer s = Hub.server; boolean on = s != null;
        statusText.setText(on ? Hub.t("Proxy ligado", "Proxy running") : Hub.t("Proxy desligado", "Proxy stopped"));
        statusSub.setText(on ? Hub.t("Relógios: ", "Watches: ") + s.clients.get() + "  ·  " + Hub.t("pedidos: ", "requests: ") + s.requests.get() + "  ·  " + Hub.t("porta ", "port ") + Hub.PORT
                             : Hub.t("Toque em Ligar proxy para começar", "Tap Start proxy to begin"));
        statusDot.setBackground(shape(on ? cOk : 0xFFFFB4AB, 6));
        toggle.setText(on ? Hub.t("Desligar proxy", "Stop proxy") : Hub.t("Ligar proxy", "Start proxy"));
        toggle.setBackground(shape(on ? cBad : cAccent, 14)); toggle.setTextColor(on ? 0xFFFFFFFF : cAccentText);
        addresses.setText(Hub.t("Endereços deste celular: ", "This phone's addresses: ") + localAddresses());
        boolean wo = Hub.watch.isOpen();
        watchState.setText((wo ? "● " : "○ ") + (wo ? Hub.t("Relógio conectado", "Watch connected") : Hub.t("Relógio desconectado", "Watch disconnected"))); watchState.setTextColor(wo ? cOk : cMuted);
        groqState.setText(Hub.groqKey(this).isEmpty() ? Hub.t("Sem chave: o app Internet funciona, o chat de IA não.", "No key: Internet works, AI chat doesn't.") : Hub.t("Chave salva · modelo ", "Key saved · model ") + Hub.groqModel(this));
        String lg = Hub.logText(); logView.setText(lg.isEmpty() ? Hub.t("(vazio)", "(empty)") : lg);
    }
    private String localAddresses() {
        StringBuilder b = new StringBuilder();
        try { for (NetworkInterface ni : Collections.list(NetworkInterface.getNetworkInterfaces())) for (InetAddress a : Collections.list(ni.getInetAddresses()))
            if (a instanceof Inet4Address && !a.isLoopbackAddress()) b.append(ni.getName()).append(' ').append(a.getHostAddress()).append("   "); } catch (Exception ignored) {}
        return b.length() == 0 ? "-" : b.toString();
    }

    private void openTethering() {
        try { startActivity(new Intent("android.settings.TETHER_SETTINGS")); }
        catch (Exception e) { try { startActivity(new Intent(Settings.ACTION_WIRELESS_SETTINGS)); } catch (Exception e2) { Toast.makeText(this, Hub.t("Abra Ajustes > Conexões > Ponto de acesso e ancoragem", "Open Settings > Connections > Hotspot and tethering"), Toast.LENGTH_LONG).show(); } }
    }
    private void toggleProxy() {
        Intent i = new Intent(this, ProxyService.class);
        if (Hub.server == null) { if (Build.VERSION.SDK_INT >= 26) startForegroundService(i); else startService(i); Hub.log(Hub.t("ligando proxy...", "starting proxy...")); }
        else { i.setAction("stop"); startService(i); }
        ui.postDelayed(new Runnable() { public void run() { refresh(); } }, 600);
    }

    private void saveGroq() { Hub.saveGroq(this, groqKey.getText().toString(), groqModel.getText().toString()); Hub.log(Hub.t("Groq salvo", "Groq saved")); refresh(); }
    private void pasteKey() {
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (cm != null && cm.hasPrimaryClip() && cm.getPrimaryClip().getItemCount() > 0) { CharSequence t = cm.getPrimaryClip().getItemAt(0).coerceToText(this); if (t != null) groqKey.setText(t.toString().trim()); }
    }
    private void testAi() {
        saveGroq(); final Context app = getApplicationContext();
        new Thread(new Runnable() { public void run() {
            Groq g = new Groq(new Groq.Config() { public String key() { return Hub.groqKey(app); } public String model() { return Hub.groqModel(app); } public String system() { return ""; } });
            String r = new String(g.ask(Hub.t("Responda só com a palavra: ok", "Reply with just the word: ok"), new Groq.Session()), StandardCharsets.UTF_8);
            String[] parts = r.split("\n", 5); Hub.log("Groq: " + (parts.length > 4 ? parts[4].replace('\n', ' ') : r));
        } }).start();
    }

    private void connectWatch() {
        final int pos = devices.getSelectedItemPosition(); if (pos < 0 || pos >= bonded.size()) { Hub.log(Hub.t("Escolha um relógio pareado", "Pick a paired watch")); return; }
        final BluetoothDevice d = bonded.get(pos); Hub.saveWatch(this, d.getAddress());
        new Thread(new Runnable() { public void run() {
            try { Hub.watch.open(d); Hub.log(Hub.t("Relógio conectado: ", "Watch connected: ") + d.getAddress()); } catch (Exception e) { Hub.log(Hub.t("Falha ao conectar: ", "Connect failed: ") + e.getMessage()); }
        } }).start();
    }
    private void at(final String cmd) {
        new Thread(new Runnable() { public void run() {
            try { String r = Hub.watch.at(cmd, 3000); Hub.log("AT^" + cmd + " -> " + (r.isEmpty() ? "(sem resposta)" : r.replace("\r", "").replace('\n', ' '))); } catch (Exception e) { Hub.log("AT^" + cmd + ": " + e.getMessage()); }
        } }).start();
    }
    private void info() {
        new Thread(new Runnable() { public void run() {
            String[] cmds = {"GET_DEVINFO", "SWVER", "BTMAC", "GETBATPERCENT", "GETVBAT"};
            for (String c : cmds) { try { String r = Hub.watch.at(c, 2500); Hub.log(c + ": " + (r.isEmpty() ? "-" : r.replace("\r", "").replace('\n', ' '))); } catch (Exception e) { Hub.log(c + ": " + e.getMessage()); break; } }
        } }).start();
    }
    private void confirmReboot() {
        new AlertDialog.Builder(this).setTitle(Hub.t("Reiniciar o relógio?", "Reboot the watch?")).setMessage(Hub.t("O relógio vai desligar e ligar de novo.", "The watch will turn off and on again."))
            .setPositiveButton(Hub.t("Reiniciar", "Reboot"), new android.content.DialogInterface.OnClickListener() { public void onClick(android.content.DialogInterface d, int w) { at("REBOOT"); } })
            .setNegativeButton(Hub.t("Cancelar", "Cancel"), null).show();
    }
    private void readFile() {
        final String path = filePath.getText().toString().trim();
        new Thread(new Runnable() { public void run() {
            try {
                byte[] d = Hub.watch.readFile(path);
                if (d == null) { Hub.log(path + ": " + Hub.t("não existe", "not found")); return; }
                boolean text = true; for (int i = 0; i < Math.min(d.length, 400); i++) { int c = d[i] & 255; if (c < 9 || (c > 13 && c < 32)) { text = false; break; } }
                String shown = text ? new String(d, 0, Math.min(d.length, 600), StandardCharsets.UTF_8) : "(binario)";
                Hub.log(path + " (" + d.length + " bytes): " + shown.replace('\n', ' '));
            } catch (Exception e) { Hub.log(path + ": " + e.getMessage()); }
        } }).start();
    }
}
