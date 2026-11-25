import SwiftUI
import Combine
import AppKit
final class Settings: ObservableObject {
 @Published var gainA:Double=0.1
 @Published var gainB:Double=0.1
 @Published var bufferMS:Double=64
 @Published var status="Offline"
 let directory=FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/Audio2DJ")
 init(){try? FileManager.default.createDirectory(at:directory,withIntermediateDirectories:true);if let data=try? Data(contentsOf:directory.appendingPathComponent("settings.plist")),let p=try? PropertyListSerialization.propertyList(from:data,format:nil) as? [String:Double]{gainA=p["gainA"] ?? 0.1;gainB=p["gainB"] ?? 0.1;bufferMS=p["bufferMS"] ?? 64}}
 func save(){let p=["gainA":gainA,"gainB":gainB,"bufferMS":bufferMS];do{let data=try PropertyListSerialization.data(fromPropertyList:p,format:.binary,options:0);try data.write(to:directory.appendingPathComponent("settings.plist"),options:.atomic)}catch{status="Save failed: \(error.localizedDescription)"}}
 @Published var peaks=[Double](repeating:0,count:4)
 @Published var connected=false
 @Published var underruns:UInt64=0
 @Published var dropped:UInt64=0
 func refresh(){let url=directory.appendingPathComponent("status.json");let attributes=try? FileManager.default.attributesOfItem(atPath:url.path);connected=(attributes?[.modificationDate] as? Date).map{Date().timeIntervalSince($0)<3} ?? false;if let data=try? Data(contentsOf:url),let p=try? JSONSerialization.jsonObject(with:data) as? [String:Double]{underruns=UInt64(max(0,p["underruns"] ?? 0));dropped=UInt64(max(0,p["droppedFrames"] ?? 0));let keys=["peakAL","peakAR","peakBL","peakBR"];for i in 0..<4{peaks[i]=connected ? max(p[keys[i]] ?? 0,peaks[i]*0.82):0}}}
}
private let accent=Color(red:0.24,green:0.60,blue:1)
struct LEDBar:View {
 let label:String
 let level:Double
 var body:some View{HStack(spacing:4){Text(label).font(.system(size:9,weight:.bold,design:.monospaced)).foregroundStyle(.secondary).frame(width:9);ForEach(0..<12){i in let threshold=pow(10.0,Double(-60+i*5)/20.0);let active=level>=threshold;RoundedRectangle(cornerRadius:1.5).fill(active ? (i>=11 ? Color.red:i>=9 ? Color.orange:accent):Color.white.opacity(0.07)).frame(height:5)}}.accessibilityElement(children:.ignore).accessibilityLabel("\(label) level").accessibilityValue(level>0 ? String(format:"%.0f dB",20*log10(level)):"Silent")}
}

struct OutputCard:View {
 let name:String
 @Binding var gain:Double
 let left:Double
 let right:Double
 var save:()->Void
 @State private var remembered:Double=0.1
 @State private var lastTick:Int = -1
 @State private var lastFeedback:Double=0
 @State private var editing=false
 var body:some View {
  VStack(alignment:.leading,spacing:18){
   HStack{Text("OUTPUT").font(.system(size:10,weight:.bold,design:.rounded)).tracking(2).foregroundStyle(.secondary);Text(name).font(.system(size:22,weight:.semibold,design:.rounded));Spacer();Button{if gain>0{remembered=gain;gain=0}else{gain=max(remembered,0.01)};save();NSHapticFeedbackManager.defaultPerformer.perform(.levelChange,performanceTime:.now)}label:{Image(systemName:gain==0 ? "speaker.slash.fill":"speaker.wave.2.fill").frame(width:28,height:28).foregroundStyle(gain==0 ? .secondary:accent)}.buttonStyle(.plain).help(gain==0 ? "Unmute output \(name)":"Mute output \(name)").accessibilityLabel("Mute output \(name)")}
   HStack(alignment:.firstTextBaseline){Text(String(format:"%.0f",gain*100)).font(.system(size:48,weight:.light,design:.rounded)).monospacedDigit();Text("%").foregroundStyle(.secondary);Spacer();Text(gain==0 ? "−∞ dB":String(format:"%.1f dB",20*log10(gain))).font(.system(size:13,weight:.medium,design:.monospaced)).foregroundStyle(.secondary)}
   Slider(value:$gain,in:0...1,onEditingChanged:{active in editing=active;if active{lastTick=Int((gain*20).rounded())}else{NSHapticFeedbackManager.defaultPerformer.perform(.levelChange,performanceTime:.now)}}).tint(accent).accessibilityLabel("Output \(name) volume").onChange(of:gain){_,value in
    save();let tick=Int((value*20).rounded());let now=CFAbsoluteTimeGetCurrent();if editing&&tick != lastTick&&now-lastFeedback>0.04{NSHapticFeedbackManager.defaultPerformer.perform(.alignment,performanceTime:.now);lastFeedback=now};lastTick=tick
   }
   VStack(spacing:6){LEDBar(label:"L",level:left);LEDBar(label:"R",level:right)}
  }.padding(22).background(RoundedRectangle(cornerRadius:18).fill(Color.white.opacity(0.045))).overlay(RoundedRectangle(cornerRadius:18).strokeBorder(Color.white.opacity(0.08)))
 }
}
struct Content:View {
 @StateObject var settings=Settings()
 let timer=Timer.publish(every:0.05,on:.main,in:.common).autoconnect()
 var body:some View {
  VStack(alignment:.leading,spacing:22){
   HStack(spacing:14){if let url=Bundle.main.url(forResource:"AppIcon",withExtension:"icns"),let image=NSImage(contentsOf:url){Image(nsImage:image).resizable().frame(width:58,height:58)};VStack(alignment:.leading,spacing:4){Text("AUDIO 2 DJ").font(.system(size:23,weight:.semibold,design:.rounded)).tracking(1);Text("CONTROL PANEL").font(.system(size:10,weight:.medium)).tracking(3).foregroundStyle(.secondary)};Spacer();HStack(spacing:6){Circle().fill(settings.connected ? accent:Color.gray).frame(width:6,height:6);Text(settings.connected ? "Connected":"Offline").font(.system(size:11,weight:.medium)).foregroundStyle(.secondary)}}
   HStack(spacing:14){OutputCard(name:"A",gain:$settings.gainA,left:settings.peaks[0],right:settings.peaks[1],save:settings.save);OutputCard(name:"B",gain:$settings.gainB,left:settings.peaks[2],right:settings.peaks[3],save:settings.save)}
   HStack{Text("Buffer").font(.system(size:13,weight:.medium));Spacer();Picker("Buffer",selection:$settings.bufferMS){Text("32 ms").tag(32.0);Text("64 ms").tag(64.0);Text("128 ms").tag(128.0)}.labelsHidden().pickerStyle(.segmented).frame(width:250).onChange(of:settings.bufferMS){_,_ in settings.save()}}
   HStack{Text("USB 48 kHz");Spacer();Text("Underruns \(settings.underruns)");Text("Dropped \(settings.dropped)")}.font(.system(size:10,weight:.medium,design:.monospaced)).foregroundStyle(.secondary)
  }.padding(26).frame(width:650).background(LinearGradient(colors:[Color(red:0.025,green:0.085,blue:0.16),Color(red:0.025,green:0.035,blue:0.055)],startPoint:.topTrailing,endPoint:.bottomLeading)).preferredColorScheme(.dark).onReceive(timer){_ in settings.refresh()}.onAppear{settings.refresh()}
 }
}
final class AppDelegate:NSObject,NSApplicationDelegate {
 func applicationDidFinishLaunching(_ notification:Notification){
  if let url=Bundle.main.url(forResource:"AppIcon",withExtension:"icns"),let icon=NSImage(contentsOf:url){NSApplication.shared.applicationIconImage=icon}
 }
}
@main struct Control:App {@NSApplicationDelegateAdaptor(AppDelegate.self) var delegate;var body:some Scene{WindowGroup("Audio 2 DJ Control"){Content()}.windowResizability(.contentSize)}}
