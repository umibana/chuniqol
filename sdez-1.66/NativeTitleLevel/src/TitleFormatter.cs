using System.Globalization;
public static class TitleFormatter {
 public static string Format(string title,int level,int levelDecimal,bool enabled) {
  if(string.IsNullOrEmpty(title)||!enabled||level<=0||levelDecimal<0||levelDecimal>9)return title;
  return title+" ["+level.ToString(CultureInfo.InvariantCulture)+"."+levelDecimal.ToString(CultureInfo.InvariantCulture)+"]";
 }
}