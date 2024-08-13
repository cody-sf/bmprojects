//
//  UmbrellaApp.swift
//  Umbrella Watch App
//
//  Created by Cody LaRocque on 2/7/24.
//

import SwiftUI
import CoreBluetooth
import Foundation

// data
//private var centralManager: CBCentralManager!
//private var bluefruitPeripheral: CBPeripheral!
//private var txCharacteristic: CBCharacteristic!
//private var rxCharacteristic: CBCharacteristic!
//private var peripheralArray: [CBPeripheral] = []
//private var rssiArray = [NSNumber]()
//private var timer = Timer()
//
//
//struct CBUUIDs{
//    static let kBLEService_UUID = "99f54e09-8916-4083-adce-bcd996e9510"
//    static let kBLE_Characteristic_uuid_Tx = "f5353f5a-0cf1-4253-bccd-4c06b2bf339d"
//    //    static let kBLE_Characteristic_uuid_Rx = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
//    
//    static let BLEService_UUID = CBUUID(string: kBLEService_UUID)
//    static let BLE_Characteristic_uuid_Tx = CBUUID(string: kBLE_Characteristic_uuid_Tx)//(Property = Write without response)
//    //    static let BLE_Characteristic_uuid_Rx = CBUUID(string: kBLE_Characteristic_uuid_Rx)// (Property = Read/Notify)
//}
//
//func connectToDevice() -> Void {
//  centralManager?.connect(bluefruitPeripheral!, options: nil)
//}
//
//func startScanning() -> Void {
//    // Remove prior data
//    peripheralArray.removeAll()
//    rssiArray.removeAll()
//    // Start Scanning
//    centralManager?.scanForPeripherals(withServices: [CBUUIDs.BLEService_UUID])
//    print("Scanning...")
//    Timer.scheduledTimer(withTimeInterval: 15, repeats: false) {_ in
//        stopScanning()
//    }
//}
//
//func stopTimer() -> Void {
//  // Stops Timer
//  timer.invalidate()
//}
//
//func stopScanning() -> Void {
//    print("stopped scanning");
//    centralManager?.stopScan()
//}
//
//// MARK: - Check
//func centralManagerDidUpdateState(_ central: CBCentralManager) {
//    print("State Updated!")
//  switch central.state {
//    case .poweredOff:
//        print("Is Powered Off.")
//
//    
//
//    case .poweredOn:
//        print("Is Powered On.")
//        startScanning()
//    case .unsupported:
//        print("Is Unsupported.")
//    case .unauthorized:
//    print("Is Unauthorized.")
//    case .unknown:
//        print("Unknown")
//    case .resetting:
//        print("Resetting")
//    @unknown default:
//      print("Error")
//    }
//}
//
//// MARK: - Discover
//func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
//  print("Function: \(#function),Line: \(#line)")
//
//  bluefruitPeripheral = peripheral
//
//  if peripheralArray.contains(peripheral) {
//      print("Duplicate Found.")
//  } else {
//    peripheralArray.append(peripheral)
//    rssiArray.append(RSSI)
//  }
//
//  print( "Peripherals Found: \(peripheralArray.count)")
//  print("Peripheral Discovered: \(peripheral)")
//
//}
//
//// MARK: - Connect
//func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
//    stopScanning()
//    bluefruitPeripheral.discoverServices([CBUUIDs.BLEService_UUID])
//}

@main
struct Umbrella_Watch_AppApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
