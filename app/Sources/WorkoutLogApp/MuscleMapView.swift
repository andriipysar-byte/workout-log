import SwiftUI
import WebKit

/// WKWebView is the simplest way to render arbitrary SVG on macOS.
struct MuscleMapView: NSViewRepresentable {
    let svg: String

    func makeNSView(context: Context) -> WKWebView {
        let web = WKWebView()
        web.setValue(false, forKey: "drawsBackground")   // transparent, blend with the card
        return web
    }

    func updateNSView(_ web: WKWebView, context: Context) {
        let html = """
        <!doctype html><html><head><meta name="viewport" content="width=device-width">
        <style>html,body{margin:0;height:100%;background:transparent}
        svg{width:100%;height:100%;display:block}</style></head>
        <body>\(svg)</body></html>
        """
        web.loadHTMLString(html, baseURL: nil)
    }
}
