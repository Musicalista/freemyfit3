package com.fit3.hub;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** One screen: the watch proxy (start/stop), the Groq settings, and actions on the watch over Bluetooth serial. */
public final class MainActivity extends Activity {
    private TextView status, addresses, logView, watchState;
    private Button toggle;
    private EditText groqKey, groqModel, filePath;
    private Spinner devices, brightness, language;
    private final List<BluetoothDevice> bonded = new ArrayList<>();
    private final Handler ui = new Handler(Looper.getMainLooper());
    private static final String[] LANG_NAMES = {"Português (Brasil)", "English", "English (US)", "Português (Portugal)"};
    private static final int[] LANG_IDS = {68, 30, 13, 52};

    private int dp(int v) { return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()); }
    private TextView title(String s) { TextView t = new TextView(this); t.setText(s); t.setTextSize(18); t.setTypeface(Typeface.DEFAULT_BOLD); t.setPadding(0, dp(18), 0, dp(4)); return t; }
    private TextView text(String s) { TextView t = new TextView(this); t.setText(s); t.setTextSize(14); return t; }
    private Button button(String s, View.OnClickListener l) { Button b = new Button(this); b.setText(s); b.setAllCaps(false); b.setOnClickListener(l); return b; }
    private LinearLayout row(View... v) { LinearLayout r = new LinearLayout(this); r.setOrientation(LinearLayout.HORIZONTAL); for (View x : v) r.addView(x, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)); return r; }

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        ScrollView sv = new ScrollView(this); LinearLayout col = new LinearLayout(this); col.setOrientation(LinearLayout.VERTICAL); col.setPadding(dp(16), dp(12), dp(16), dp(24)); sv.addView(col);
        setContentView(sv);

        // ---- proxy
        col.addView(title(Hub.t("Proxy do relógio", "Watch proxy")));
        status = text(""); col.addView(status);
        addresses = text(""); addresses.setTextSize(12); col.addView(addresses);
        toggle = button("", new View.OnClickListener() { public void onClick(View v) { toggleProxy(); } }); col.addView(toggle);
        col.addView(text(Hub.t("Com a ancoragem Bluetooth ligada, o relógio acessa este proxy pelo endereço do próprio celular (porta " + Hub.PORT + "). Não precisa de PC nem de Wi-Fi.",
                "With Bluetooth tethering on, the watch reaches this proxy at the phone's own address (port " + Hub.PORT + "). No PC or Wi-Fi needed.")));

        // ---- groq
        col.addView(title("Groq (IA)"));
        groqKey = new EditText(this); groqKey.setHint(Hub.t("Chave da API (gsk_...)", "API key (gsk_...)")); groqKey.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD); groqKey.setText(Hub.groqKey(this)); col.addView(groqKey);
        groqModel = new EditText(this); groqModel.setHint(Hub.t("Modelo", "Model")); groqModel.setText(Hub.groqModel(this)); col.addView(groqModel);
        col.addView(row(button(Hub.t("Colar chave", "Paste key"), new View.OnClickListener() { public void onClick(View v) { pasteKey(); } }),
                        button(Hub.t("Salvar", "Save"), new View.OnClickListener() { public void onClick(View v) { Hub.saveGroq(MainActivity.this, groqKey.getText().toString(), groqModel.getText().toString()); Hub.log(Hub.t("Groq salvo", "Groq saved")); } }),
                        button(Hub.t("Testar IA", "Test AI"), new View.OnClickListener() { public void onClick(View v) { testAi(); } })));

        // ---- watch
        col.addView(title(Hub.t("Relógio (Bluetooth)", "Watch (Bluetooth)")));
        devices = new Spinner(this); col.addView(devices);
        watchState = text(""); col.addView(watchState);
        col.addView(row(button(Hub.t("Conectar", "Connect"), new View.OnClickListener() { public void onClick(View v) { connectWatch(); } }),
                        button(Hub.t("Desconectar", "Disconnect"), new View.OnClickListener() { public void onClick(View v) { Hub.watch.close(); refresh(); } })));
        col.addView(row(button(Hub.t("Informações", "Info"), new View.OnClickListener() { public void onClick(View v) { info(); } }),
                        button(Hub.t("Notificação de teste", "Test notification"), new View.OnClickListener() { public void onClick(View v) { at("NOTICE=-a=Fit3 Hub: " + Hub.t("notificacao de teste", "test notification")); } })));
        brightness = new Spinner(this); brightness.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, new String[] {"20", "40", "60", "80", "100"})); brightness.setSelection(2);
        col.addView(row(text(Hub.t("Brilho (%)", "Brightness (%)")), brightness, button("OK", new View.OnClickListener() { public void onClick(View v) { at("SCREENBRIGHT=" + brightness.getSelectedItem()); } })));
        language = new Spinner(this); language.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, LANG_NAMES));
        col.addView(row(text(Hub.t("Idioma", "Language")), language, button("OK", new View.OnClickListener() { public void onClick(View v) { at("LANGUAGE=" + LANG_IDS[language.getSelectedItemPosition()]); } })));
        col.addView(row(button(Hub.t("Vibrar", "Vibrate"), new View.OnClickListener() { public void onClick(View v) { at("MOTOR_VIB=on"); } }),
                        button(Hub.t("Parar vibração", "Stop vibration"), new View.OnClickListener() { public void onClick(View v) { at("MOTOR_VIB=off"); } }),
                        button(Hub.t("Reiniciar", "Reboot"), new View.OnClickListener() { public void onClick(View v) { confirmReboot(); } })));
        filePath = new EditText(this); filePath.setText("/user/notice_reply_data.txt"); col.addView(filePath);
        col.addView(row(button(Hub.t("Ler arquivo", "Read file"), new View.OnClickListener() { public void onClick(View v) { readFile(); } })));

        // ---- log
        col.addView(title("Log"));
        logView = text(""); logView.setTypeface(Typeface.MONOSPACE); logView.setTextSize(11); logView.setTextIsSelectable(true); col.addView(logView);

        Hub.listener = new Hub.Listener() { public void changed() { ui.post(new Runnable() { public void run() { refresh(); } }); } };
        askPermissions(); loadDevices(); refresh();
        ui.postDelayed(new Runnable() { public void run() { refresh(); ui.postDelayed(this, 2000); } }, 2000);
    }

    @Override protected void onDestroy() { Hub.listener = null; super.onDestroy(); }

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
        SecureServer s = Hub.server;
        status.setText(s == null ? Hub.t("Proxy parado", "Proxy stopped") : Hub.t("Proxy ligado — relógios conectados: ", "Proxy running — connected watches: ") + s.clients.get() + Hub.t(", pedidos: ", ", requests: ") + s.requests.get());
        toggle.setText(s == null ? Hub.t("Ligar proxy", "Start proxy") : Hub.t("Desligar proxy", "Stop proxy"));
        addresses.setText(Hub.t("Endereços deste celular: ", "This phone's addresses: ") + localAddresses());
        watchState.setText(Hub.watch.isOpen() ? Hub.t("Relógio conectado", "Watch connected") : Hub.t("Relógio desconectado", "Watch disconnected"));
        logView.setText(Hub.logText());
    }
    private String localAddresses() {
        StringBuilder b = new StringBuilder();
        try { for (NetworkInterface ni : Collections.list(NetworkInterface.getNetworkInterfaces())) for (InetAddress a : Collections.list(ni.getInetAddresses()))
            if (a instanceof Inet4Address && !a.isLoopbackAddress()) b.append(ni.getName()).append(' ').append(a.getHostAddress()).append("  "); } catch (Exception ignored) {}
        return b.length() == 0 ? "-" : b.toString();
    }

    private void toggleProxy() {
        Intent i = new Intent(this, ProxyService.class);
        if (Hub.server == null) { if (Build.VERSION.SDK_INT >= 26) startForegroundService(i); else startService(i); Hub.log(Hub.t("ligando proxy...", "starting proxy...")); }
        else { i.setAction("stop"); startService(i); }
        ui.postDelayed(new Runnable() { public void run() { refresh(); } }, 600);
    }

    private void pasteKey() {
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (cm != null && cm.hasPrimaryClip() && cm.getPrimaryClip().getItemCount() > 0) { CharSequence t = cm.getPrimaryClip().getItemAt(0).coerceToText(this); if (t != null) groqKey.setText(t.toString().trim()); }
    }
    private void testAi() {
        Hub.saveGroq(this, groqKey.getText().toString(), groqModel.getText().toString()); final Context app = getApplicationContext();
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
            .setPositiveButton(Hub.t("Reiniciar", "Reboot"), new DialogInterface.OnClickListener() { public void onClick(DialogInterface d, int w) { at("REBOOT"); } })
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
