package org.clanbomber.game;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class ClanBomberActivity extends SDLActivity {
    @Override protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_image", "SDL2_mixer", "SDL2_ttf", "main"};
    }

    private void copyAssets(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!target.isDirectory() && !target.mkdirs()) throw new IOException("Cannot create " + target);
            for (String child : children) copyAssets(assets, path + "/" + child, new File(target, child));
        } else {
            try (InputStream in = assets.open(path); FileOutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[16384]; int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
            }
        }
    }

    @Override protected void onCreate(Bundle state) {
        try {
            copyAssets(getAssets(), "ClanBomber2", new File(getFilesDir(), "ClanBomber2"));
        } catch (IOException error) {
            throw new IllegalStateException("Unable to prepare game assets", error);
        }
        super.onCreate(state);
        FrameLayout controls = new FrameLayout(this);
        controls.setMotionEventSplittingEnabled(true);
        mLayout.addView(controls, new android.widget.RelativeLayout.LayoutParams(-1, -1));
        int size = (int)(64 * getResources().getDisplayMetrics().density);
        addControl(controls, "▲", KeyEvent.KEYCODE_DPAD_UP, Gravity.LEFT | Gravity.BOTTOM, size, 2*size, size);
        addControl(controls, "◀", KeyEvent.KEYCODE_DPAD_LEFT, Gravity.LEFT | Gravity.BOTTOM, 0, size, size);
        addControl(controls, "▼", KeyEvent.KEYCODE_DPAD_DOWN, Gravity.LEFT | Gravity.BOTTOM, size, size, size);
        addControl(controls, "▶", KeyEvent.KEYCODE_DPAD_RIGHT, Gravity.LEFT | Gravity.BOTTOM, 2*size, size, size);
        addControl(controls, "BOMB / OK", KeyEvent.KEYCODE_ENTER, Gravity.RIGHT | Gravity.BOTTOM, size/3, size, size + size/2);
        addControl(controls, "BACK", KeyEvent.KEYCODE_DEL, Gravity.RIGHT | Gravity.TOP, size/4, size/4, size);
        addControl(controls, "PAUSE", KeyEvent.KEYCODE_P, Gravity.LEFT | Gravity.TOP, size/4, size/4, size);
    }

    private void addControl(FrameLayout parent, String label, int key, int gravity,
                            int horizontal, int vertical, int size) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextSize(12);
        button.setAllCaps(false);
        button.setAlpha(0.72f);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(size, size, gravity);
        if ((gravity & Gravity.RIGHT) == Gravity.RIGHT) params.rightMargin = horizontal;
        else params.leftMargin = horizontal;
        if ((gravity & Gravity.BOTTOM) == Gravity.BOTTOM) params.bottomMargin = vertical;
        else params.topMargin = vertical;
        button.setOnTouchListener((View view, MotionEvent event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: SDLActivity.onNativeKeyDown(key); return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL: SDLActivity.onNativeKeyUp(key); return true;
                default: return true;
            }
        });
        parent.addView(button, params);
    }
}
