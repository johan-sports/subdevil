# Subdevil

Cross-platform USB metadata. Inspired by [CargoSense's USBDriver](https://github.com/CargoSense/usb-driver).

## Platforms

* OSX
* Windows
* ~~Linux~~

## Requirements

* Node.js >= v5.x

## Install

From NPM:

```
$ npm install subdevil
```

From git:

```
$ npm install git://github.com/johan-sports/subdevil
```

## Usage

Include the subdevil library:

```javascript
var subdevil = require('subdevil');
```

Poll and print available USB devices:

```javascript
subdevil.poll().then(function(devices) {
  console.log(devices.length + ' USB devices connected.');
  devices.forEach(function(dev) {
    console.log('====================');
    console.log(dev);
    console.log('====================');
  }):
}).catch(function(error){
  console.log('Something went wrong: ' + error);
});
```

Get a specific USB device by ID:

```javascript
subdevil.get('0x22B3-0xEF23-IDQ21AS23AB').then(function(dev) {
  console.log('Device found: ' + dev)
});
```

Unmount a device (if mounted):

```javascript
subdevil.unmount('0x22B3-0xEF23-IDQ21AS23AB').then(function() {
  console.log('Device unmounted successfully');
}).catch(function() { 
  console.log('Device unmount failed'); 
});
```

Set the log file to use for debug information:

```javascript
subdevil.setLogFile('subdevil-debug.log');
```

A device is represented as an object containing these attributes:

```javascript
{
  id: '0x22B3-0xEF23-IDQ21AS23AB', // Unique ID for attached device
  vendorCode: '0x22B3',            // Hex for USB vendor ID
  productCode: '0xEF23',           // Hex for USB product ID
  manufacturer: 'Foo Bar Technologies', // Name of manufacturer, if available
  product: 'Code-o-meter 3000',    // Name of product, if available
  serialNumber: 'IDQ21AS23AB',     // Serial number of device, if available
  mount: '/Volumes/MY_FILES'       // Path to volume mount point, if available
}
```

## Test

```
npm test
```

## Contributing

Contributions are more than welcome. Make sure the tests pass, open a PR and 
add yourself to the [CONTRIBUTORS] file.

[CONTRIBUTORS]: https://github.com/johan-sports/subdevil/blob/master/CONTRIBUTORS

## License

See [LICENSE](https://github.com/johan-sports/subdevil/blob/master/LICENSE)
