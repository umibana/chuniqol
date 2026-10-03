using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using HarmonyLib;
using MelonLoader;
[assembly: MelonInfo(typeof(NativeTitleLevel), "NativeTitleLevel", "1.0.0", "Local mod")]
[assembly: MelonGame("sega-interactive", "Sinmai")]
public class NativeTitleLevel : MelonMod {
 static FieldInfo scoreData;
 static PropertyInfo level,levelDecimal,isEnable;
 static bool warned;
 public override void OnInitializeMelon() {
  try {
   var data=AccessTools.TypeByName("Process.MusicSelectProcess+MusicSelectData");
   var notes=AccessTools.TypeByName("Manager.MaiStudio.Notes");
   scoreData=AccessTools.Field(data,"ScoreData");
   level=AccessTools.Property(notes,"level");levelDecimal=AccessTools.Property(notes,"levelDecimal");isEnable=AccessTools.Property(notes,"isEnable");
   var target=AccessTools.Method(AccessTools.TypeByName("Monitor.MusicSelect.ChainList.MusicSelectChainList"),"SetChainData");
   var body=target.GetMethodBody();
   if(body.LocalVariables.Count<4||body.LocalVariables[3].LocalType!=data||scoreData==null||level==null||levelDecimal==null||isEnable==null)
    throw new InvalidOperationException("Unsupported game metadata/layout");
   new HarmonyLib.Harmony("local.NativeTitleLevel").Patch(target,transpiler:new HarmonyMethod(typeof(NativeTitleLevel).GetMethod("Transpile")));
   MelonLogger.Msg("NativeTitleLevel enabled: native song-card title, selected chart decimal, native scrolling.");
  }catch(Exception e){MelonLogger.Error("NativeTitleLevel disabled: "+e.Message);}
 }
 public static IEnumerable<CodeInstruction> Transpile(IEnumerable<CodeInstruction> instructions) {
  var code=new List<CodeInstruction>(instructions);int match=-1,count=0;
  for(int i=3;i<code.Count;i++) {
   var name=code[i-1].operand as MethodInfo;var str=code[i].operand as MethodInfo;
   var music=code[i-2].operand as FieldInfo;
   if(name!=null&&name.Name=="get_name"&&name.DeclaringType.FullName=="Manager.MaiStudio.MusicData"&&str!=null&&str.Name=="get_str"&&music!=null&&music.Name=="MusicData"&&code[i-3].opcode==OpCodes.Ldloc_3){match=i;count++;}
  }
  if(count!=1)throw new InvalidOperationException("Expected exactly one native card title; found "+count);
  code.InsertRange(match+1,new[]{new CodeInstruction(OpCodes.Ldloc_3),new CodeInstruction(OpCodes.Ldarg_S,(byte)4),new CodeInstruction(OpCodes.Call,typeof(NativeTitleLevel).GetMethod("Decorate"))});
  MelonLogger.Msg("NativeTitleLevel patched one song-title source in SetChainData.");
  return code;
 }
 public static string Decorate(string title,object selectedMusic,int difficulty) {
  try {
   if(selectedMusic==null||scoreData==null)return title;
   var scores=scoreData.GetValue(selectedMusic) as IList;
   if(scores==null||difficulty<0||difficulty>=scores.Count||scores[difficulty]==null)return title;
   var note=scores[difficulty];
   return TitleFormatter.Format(title,(int)level.GetValue(note,null),(int)levelDecimal.GetValue(note,null),(bool)isEnable.GetValue(note,null));
  }catch(Exception e){if(!warned){warned=true;MelonLogger.Warning("NativeTitleLevel kept original title: "+e.Message);}return title;}
 }
}