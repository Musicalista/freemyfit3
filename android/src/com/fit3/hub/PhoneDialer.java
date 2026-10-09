package com.fit3.hub;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.telecom.TelecomManager;

/** Places the call the watch asked for (needs the CALL_PHONE permission, granted once in the app). */
final class PhoneDialer implements SecureServer.Dialer {
    private final Context ctx;
    PhoneDialer(Context c) { ctx = c; }
    @Override public void dial(String number) throws Exception {
        if (Build.VERSION.SDK_INT >= 23 && ctx.checkSelfPermission(Manifest.permission.CALL_PHONE) != PackageManager.PERMISSION_GRANTED)
            throw new Exception(Hub.t("permita 'Telefone' ao Fit3 Hub (abra o app no celular)", "allow 'Phone' for Fit3 Hub (open the app on the phone)"));
        TelecomManager tm = (TelecomManager) ctx.getSystemService(Context.TELECOM_SERVICE);
        if (tm == null) throw new Exception(Hub.t("este celular nao tem telefone", "this phone has no telephony"));
        tm.placeCall(Uri.fromParts("tel", number, null), new Bundle());
        Hub.log(Hub.t("ligando para ", "calling ") + number);
    }
}
