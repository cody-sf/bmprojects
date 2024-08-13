//
//  ContentView.swift
//  Umbrella Watch App
//
//  Created by Cody LaRocque on 2/7/24.
//

import SwiftUI
import RxBluetoothKit
import CoreBluetooth
let manager = CentralManager(queue: .main)
let state: BluetoothState = manager.state
let disposable = manager.observeState()
     .startWith(state)
     .filter { $0 == .poweredOn }


let kBLEService_UUID = "99f54e09-8916-4083-adce-bcd996e9510"
let kBLE_Characteristic_uuid_Tx = "f5353f5a-0cf1-4253-bccd-4c06b2bf339d"
let BLEService_UUID = CBUUID(string: kBLEService_UUID)

struct ContentView: View {
    
    func startScanning(){
        manager.observeState()
             .startWith(state)
             .filter { $0 == .poweredOn }
             .flatMap { _ in manager.scanForPeripherals(withServices: [BLEService_UUID]) }
             .take(1)
             .flatMap { $0.peripheral.establishConnection() }
             .subscribe(onNext: { peripheral in
                  print("Connected to: \(peripheral)")
             })
    }
    var body: some View {
        NavigationView {
            HStack {
                List(){
                    NavigationLink(destination: ModeView()){Text("Mode").onTapGesture {
                        print("clicked!")
                    }}
                    Text("Sensitivity").onTapGesture {
                        startScanning()
                    };
                    Text("Primary Palette")
                    Text("Secondary Palette").focusable()
                }.listStyle(CarouselListStyle())
            }
        }.navigationTitle("Umbrella Settings")
    }
}

#Preview {
    ContentView()
}

struct ModeView: View {
    var body: some View{
        NavigationView{
            HStack {
                List(){
                    Text("Bitch")
                }
            }
        }.navigationTitle("Mode")
    }
}
