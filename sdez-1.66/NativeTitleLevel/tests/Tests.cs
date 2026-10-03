using System;using System.Globalization;
class Tests {
 static int failures;
 static void Check(string reason,string expected,string actual){if(expected!=actual){Console.WriteLine("FAIL "+reason+": expected="+expected+" actual="+actual);failures++;}}
 static int Main(){
 Check("selected chart decimal", "Song [11.4]",TitleFormatter.Format("Song",11,4,true));
 Check("difficulty changes on same song", "Song [13.8]",TitleFormatter.Format("Song",13,8,true));
 Check("integer constant retains decimal", "Song [12.0]",TitleFormatter.Format("Song",12,0,true));
 CultureInfo.CurrentCulture=new CultureInfo("es-ES");
 Check("dot independent of system locale", "曲名 [14.9]",TitleFormatter.Format("曲名",14,9,true));
 Check("disabled chart has no false value", "Song",TitleFormatter.Format("Song",11,4,false));
 Check("unknown level has no false value", "Song",TitleFormatter.Format("Song",-1,0,true));
 Check("invalid decimal has no false value", "Song",TitleFormatter.Format("Song",11,10,true));
 Check("empty title", "",TitleFormatter.Format("",11,4,true));
 Console.WriteLine(failures==0?"PASS 8 behavioral tests":"FAIL "+failures+" tests");return failures==0?0:1;
 }
}