package com.dfdx047.dragonrage.engine

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper

/**
 * Starts Budokai Tenkaichi 2's engine again. The engine cannot be started twice in one process (it needs addresses only a fresh one has), so
 * Bt2GameActivity.restartEngine ends its own process; this screen, in the app's main process, which stays, waits for that and opens the game
 * afresh in a new process: for an online match, whose two games must start from the same state, and again when the match is over.
 */
class Bt2RelaunchActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Handler(Looper.getMainLooper()).postDelayed({
            startActivity(Intent(this, Bt2GameActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
            finish()
        }, 1000)
    }
}
