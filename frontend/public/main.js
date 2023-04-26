const ws = new WebSocket('ws://127.0.0.1:9002')
// const ws = {}
ws.binaryType = 'arraybuffer'
let configE = null
let updateConfigBtnE = null

const INT16_MIN = -1 << (Int16Array.BYTES_PER_ELEMENT * 8 - 1) // -32768
const BIPNORM_TO_INT16_SLOPE = 1 << (Int16Array.BYTES_PER_ELEMENT * 8 - 1) // 32768
const INT16_TO_BIPNORM_SLOPE = 1 / BIPNORM_TO_INT16_SLOPE
const gStreamsMetadata = [
  {
    name: 'input',
    channelsCount: 6,
    sampleSize: 160,
    sampleCount: 300
  },
  {
    name: 'output',
    channelsCount: 1,
    sampleSize: 160,
    sampleCount: 300
  }
]

const streamsContext = []

function clear (a) {
  // Float64Array is faster but since the array may not be a multiple of 8
  // bytes, we use Int8Array to set the remaining bytes
  const fastView = new Float64Array(a.buffer, 0, (a.buffer.byteLength / Float64Array.BYTES_PER_ELEMENT) | 0)
  const byteView = new Int8Array(
    a.buffer, fastView.byteLength,
    a.buffer.byteLength % Float64Array.BYTES_PER_ELEMENT)

  const length1 = fastView.length
  const length2 = byteView.length

  for (let i = 0; i < length1; i++) {
    fastView[i] = 0
  }

  for (let i = 0; i < length2; i++) {
    byteView[i] = 0
  }
}

function int16ToBipNorm (input) {
  const inputStart = INT16_MIN
  const slope = INT16_TO_BIPNORM_SLOPE
  const outputStart = -1.0
  return outputStart + (slope * (input - inputStart))
}

ws.onerror = console.error

ws.onopen = function open () {
  ws.send('getConfig')
}

function getStreamFrameBuffer (inbuf) {
  const dv = new DataView(inbuf)
  let offset = 0
  const streamId = dv.getUint16(offset, true)
  offset += Uint16Array.BYTES_PER_ELEMENT
  const data = new Int16Array(inbuf, offset, (inbuf.byteLength - offset) / Int16Array.BYTES_PER_ELEMENT)
  offset += data.length
  return {
    streamId, data
  }
}

ws.onmessage = function message (event) {
  if (event.data instanceof ArrayBuffer) {
    const { streamId, data } = getStreamFrameBuffer(event.data)
    const streamContext = streamsContext[streamId]
    if (streamContext && streamContext.bufIdx + data.length < streamContext.buffer.length) {
      streamContext.buffer.set(data, streamContext.bufIdx)
      streamContext.bufIdx += data.length
    }
  } else {
    try {
      const evt = JSON.parse(event.data)
      processWsEvt(evt)
    } catch (e) {
      console.error('could not handle', event.data, e)
    }
  }
}

function processWsEvt ({ type, payload }) {
  switch (type) {
    case 'done':
      for (const streamContext of streamsContext) {
        for (let channelIdx = 0; channelIdx < streamContext.channels.length; channelIdx++) {
          const channel = streamContext.channels[channelIdx]
          resetCanvas(streamContext, channel)
          draw(streamContext, channel, channelIdx)
        }
        streamContext.bufIdx = 0
      }
      break
    case 'config':
      configE.value = payload.replaceAll('\\n', '\n')
      updateConfigBtnE.disabled = false
      break
  }
}

function testCanvas (canvas, ctx) {
  let cursor = 0
  ctx.beginPath()
  ctx.moveTo(0, canvas.height)
  ctx.lineTo(cursor += 50, 0)
  ctx.lineTo(cursor += 50, canvas.height)
  ctx.lineTo(cursor += 50, 0)
  ctx.lineTo(cursor += 50, canvas.height)
  ctx.lineTo(cursor += 50, 0)
  ctx.lineTo(cursor += 50, canvas.height)
  ctx.stroke()
}

function resetCanvas (streamContext, channel) {
  const { sampleSize, sampleCount } = streamContext
  const { canvas, ctx } = channel
  const cs = getComputedStyle(canvas)
  const width = parseInt(cs.getPropertyValue('width'), 10)
  const height = parseInt(cs.getPropertyValue('height'), 10)
  canvas.width = width
  canvas.height = height
  ctx.clearRect(0, 0, canvas.width, canvas.height)
  ctx.beginPath()
  ctx.moveTo(0, canvas.height / 2)
  ctx.lineTo(canvas.width, canvas.height / 2)
  ctx.stroke()
  channel.inc = canvas.width / (sampleSize * sampleCount)
}

function draw (streamContext, channel, channelIdx) {
  const { buffer, channels } = streamContext
  const channelsCount = channels.length
  const { canvas, ctx, inc } = channel
  if (!ctx) return
  ctx.beginPath()
  const canvasScale = -canvas.height / 2
  const canvasOffset = canvas.height / 2
  let tot = 0
  for (let i = 0; i < buffer.length; i++) {
    const val = buffer[channelsCount * i + channelIdx]
    const scaled1 = int16ToBipNorm(val)
    const scaled = scaled1 * canvasScale + canvasOffset
    ctx.lineTo(tot += inc, scaled)
  }
  ctx.stroke()
}

function setupCanvas (streamsMetadata) {
  const wavesContainerE = document.getElementById('wavesContainer')
  const frag = document.createDocumentFragment()
  for (const stream of streamsMetadata) {
    const { name, channelsCount, sampleSize, sampleCount } = stream
    const streamContext = {
      name,
      sampleSize,
      sampleCount,
      buffer: new Int16Array(channelsCount * sampleSize * sampleCount),
      bufIdx: 0,
      channels: []
    }
    for (let i = 0; i < stream.channelsCount; i++) {
      const canvas = document.createElement('canvas')
      const ctx = canvas.getContext('2d')
      const channel = {
        canvas,
        ctx
      }
      ctx.lineWidth = 1
      resetCanvas(streamContext, channel)
      // testCanvas(canvas, ctx)
      frag.appendChild(canvas)
      streamContext.channels.push(channel)
    }
    streamsContext.push(streamContext)
  }
  wavesContainerE.appendChild(frag)
}

function updateConfigHandler (evt) {
  const val = configE.value.replaceAll('\n', '\\n')
  ws.send(`setConfig:${val}`)
  updateConfigBtnE.disabled = true
}

function loadElements () {
  configE = document.getElementById('config')
  updateConfigBtnE = document.getElementById('updateConfigBtn')
}

function main () {
  loadElements()
  setupCanvas(gStreamsMetadata)
}

document.addEventListener('DOMContentLoaded', function () {
  main()
}, false)

window.updateConfigHandler = updateConfigHandler
