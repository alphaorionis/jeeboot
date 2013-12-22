module.exports =

  hwIds:
    '00000000000000000000000000000000':
      type: 0x100, group: 212, nodeId: 16
    '06300301c48461aeedb09351061900f5':
      type: 0x200, group: 212, nodeId: 17
    '35800405648861aee3e5ba50800200f5':
      type: 0x200, group: 212, nodeId: 18

  types:
    0x100: 'JNv6-868'
    0x200: 'MAX/69-868'
    0x300: 'JN2/69-868'

  nodes:
    'JNv6-868,212,16':
      100: 'blinkAvr1.hex'
    'MAX/69-868,212,17':
      101: 'blinkArm2.hex'
    'MAX/69-868,212,18':
      102: 'blinkArm2a.hex'
