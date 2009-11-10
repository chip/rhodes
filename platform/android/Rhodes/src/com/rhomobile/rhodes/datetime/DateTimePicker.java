package com.rhomobile.rhodes.datetime;

import android.content.Intent;

import com.rhomobile.rhodes.RhodesInstance;

public class DateTimePicker {

	public static void choose(String callback, String title, long init, int v, String opaque) {
		Intent intent = new Intent(RhodesInstance.getInstance().getApplicationContext(),
				DateTimePickerScreen.class);
		intent.putExtra("callback", callback);
		intent.putExtra("title", title);
		intent.putExtra("init", init);
		intent.putExtra("fmt", v);
		intent.putExtra("opaque", opaque);
		
		RhodesInstance.getInstance().startActivityForResult(intent, 5);
	}
	
}
