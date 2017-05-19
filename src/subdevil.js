var SubdevilNative = require('../build/Release/subdevil.node');

module.exports = {
  /**
   * Get a list of attached devices.
   */
  poll: function poll() {
    return new Promise(function(resolve) {
      resolve(SubdevilNative.poll());
    });
  },
  /**
   * Get a device by ID
   *
   * @param {String} id
   * @returns {Object}
   */
  get: function get(id) {
    return new Promise(function(resolve) {
      resolve(SubdevilNative.get(id));
    });
  },
  /**
   * Unmount a mass storage device.
   */
  unmount: function unmount(id) {
    return new Promise(function(resolve, reject) {
      if(SubdevilNative.unmount(id)) {
        resolve();
      } else {
        reject();
      }
    });
  },
  /**
   * Set the log file to use for debug information.
   */
  setLogFile: function setLogFile(filepath) {
    // TODO: Validate file path
    SubdevilNative.setLogFile(filepath);
  }
};
