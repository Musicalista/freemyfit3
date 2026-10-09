package com.fit3.hub;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

/** Foreground service that keeps the watch's proxy running while the screen is off. */
public final class ProxyService extends Service {
    private PowerManager.WakeLock wake;

    @Override public IBinder onBind(Intent i) { return null; }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && "stop".equals(intent.getAction())) { stopServer(); stopForeground(true); stopSelf(); return START_NOT_STICKY; }
        startForeground(1, notification());
        if (Hub.server == null) {
            try {
                final Context app = getApplicationContext();
                Groq groq = new Groq(new Groq.Config() {
                    public String key() { return Hub.groqKey(app); } public String model() { return Hub.groqModel(app); } public String system() { return ""; } });
                SecureServer s = new SecureServer(Hub.psk(app), Hub.PORT, groq, new SecureServer.Log() { public void log(String l) { Hub.log(l); } });
                s.dialer = new PhoneDialer(app);
                s.start(); Hub.server = s;
                PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "fit3hub:proxy"); wake.acquire();
            } catch (Exception e) { Hub.log("falha ao iniciar o proxy: " + e.getMessage()); stopSelf(); }
        }
        return START_STICKY;
    }

    private void stopServer() { SecureServer s = Hub.server; Hub.server = null; if (s != null) s.stop(); if (wake != null && wake.isHeld()) wake.release(); Hub.log("proxy parado"); }
    @Override public void onDestroy() { stopServer(); super.onDestroy(); }

    private Notification notification() {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= 26) nm.createNotificationChannel(new NotificationChannel("proxy", "Fit3 proxy", NotificationManager.IMPORTANCE_LOW));
        PendingIntent pi = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, "proxy") : new Notification.Builder(this);
        return b.setContentTitle("Fit3 Hub").setContentText(Hub.t("Proxy do relogio ligado na porta ", "Watch proxy running on port ") + Hub.PORT)
                .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth).setContentIntent(pi).setOngoing(true).build();
    }
}
