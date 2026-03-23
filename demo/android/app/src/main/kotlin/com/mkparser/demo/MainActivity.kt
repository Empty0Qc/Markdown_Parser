package com.mkparser.demo

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.mkparser.demo.ui.MainScreen
import com.mkparser.demo.ui.theme.MkParserDemoTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            MkParserDemoTheme {
                MainScreen()
            }
        }
    }
}
