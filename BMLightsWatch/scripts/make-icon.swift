#!/usr/bin/env swift
// Renders the 1024pt app icon: a palette-colored ring on black, the same shape
// the brightness control uses inside the app.
import AppKit
import CoreGraphics
import Foundation

let size = 1024
let path = CommandLine.arguments.count > 1
    ? CommandLine.arguments[1]
    : "BMLightsWatch/Assets.xcassets/AppIcon.appiconset/AppIcon.png"

let colorSpace = CGColorSpaceCreateDeviceRGB()
guard let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8,
                          bytesPerRow: 0, space: colorSpace,
                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
    fatalError("could not create bitmap context")
}

ctx.setFillColor(CGColor(red: 0.04, green: 0.04, blue: 0.05, alpha: 1))
ctx.fill(CGRect(x: 0, y: 0, width: size, height: size))

let center = CGPoint(x: size / 2, y: size / 2)
let radius = CGFloat(size) * 0.30
let lineWidth = CGFloat(size) * 0.11

// Burning Man palette sweep, matching the app's "candy"/"cosmic" family.
let stops: [(CGFloat, CGFloat, CGFloat)] = [
    (1.00, 0.65, 0.10), (1.00, 0.10, 0.87), (0.20, 0.10, 1.00),
    (0.10, 1.00, 0.69), (1.00, 0.65, 0.10),
]

// The ring is drawn as many short arcs so the color can sweep around it.
let segments = 360
ctx.setLineCap(.butt)
ctx.setLineWidth(lineWidth)
for i in 0..<segments {
    let t = CGFloat(i) / CGFloat(segments)
    let scaled = t * CGFloat(stops.count - 1)
    let index = min(Int(scaled), stops.count - 2)
    let localT = scaled - CGFloat(index)
    let from = stops[index]
    let to = stops[index + 1]
    let color = CGColor(red: from.0 + (to.0 - from.0) * localT,
                        green: from.1 + (to.1 - from.1) * localT,
                        blue: from.2 + (to.2 - from.2) * localT,
                        alpha: 1)
    ctx.setStrokeColor(color)
    let start = -CGFloat.pi / 2 + t * 2 * .pi
    let end = start + (2 * .pi / CGFloat(segments)) * 1.6
    ctx.addArc(center: center, radius: radius, startAngle: start, endAngle: end, clockwise: false)
    ctx.strokePath()
}

// The power glyph in the middle.
let glyphRadius = radius * 0.42
ctx.setStrokeColor(CGColor(red: 1, green: 1, blue: 1, alpha: 0.95))
ctx.setLineWidth(lineWidth * 0.55)
ctx.setLineCap(.round)
// The gap sits at the top, where the stem passes through it.
ctx.addArc(center: center, radius: glyphRadius,
           startAngle: .pi / 2 + 0.55, endAngle: .pi / 2 - 0.55, clockwise: false)
ctx.strokePath()
ctx.move(to: CGPoint(x: center.x, y: center.y + glyphRadius * 0.25))
ctx.addLine(to: CGPoint(x: center.x, y: center.y + glyphRadius * 1.35))
ctx.strokePath()

guard let image = ctx.makeImage() else { fatalError("could not render icon") }
let rep = NSBitmapImageRep(cgImage: image)
guard let png = rep.representation(using: .png, properties: [:]) else { fatalError("could not encode png") }
try png.write(to: URL(fileURLWithPath: path))
print("wrote \(path)")
