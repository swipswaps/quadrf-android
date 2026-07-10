package com.quadrf.usbhost;

import android.content.Context;
import android.hardware.usb.UsbManager;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertNotNull;

@RunWith(AndroidJUnit4.class)
public class UsbHostTest {

    @Test
    public void testUsbManagerExists() {
        Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        UsbManager usbManager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
        assertNotNull("UsbManager should not be null", usbManager);
    }
}
