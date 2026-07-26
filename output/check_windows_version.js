var wbemFlagReturnImmediately = 0x10;
var wbemFlagForwardOnly = 0x20;
var wmi = GetObject("winmgmts:\\\\.\\root\\CIMV2");

var isWindows11 = false;
var operatingSystems = wmi.ExecQuery(
    "SELECT Version FROM Win32_OperatingSystem", "WQL",
    wbemFlagReturnImmediately | wbemFlagForwardOnly);
var osItems = new Enumerator(operatingSystems);
for (; !osItems.atEnd(); osItems.moveNext()) {
  var version = osItems.item().Version.split(".");
  isWindows11 = version.length >= 3 && parseInt(version[0], 10) === 10 &&
      parseInt(version[2], 10) >= 22000;
  break;
}

var isNativeAmd64 = false;
var processors = wmi.ExecQuery(
    "SELECT Architecture FROM Win32_Processor", "WQL",
    wbemFlagReturnImmediately | wbemFlagForwardOnly);
var processorItems = new Enumerator(processors);
for (; !processorItems.atEnd(); processorItems.moveNext()) {
  // Win32_Processor.Architecture: 9 = x64, 12 = ARM64.
  isNativeAmd64 = processorItems.item().Architecture === 9;
  break;
}

WScript.Quit(isWindows11 && isNativeAmd64 ? 0 : 1);
