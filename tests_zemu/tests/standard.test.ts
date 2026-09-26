/** ******************************************************************************
 *  (c) 2018 - 2024 Zondax AG
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************* */

import Zemu, { zondaxMainmenuNavigation, ButtonKind, isTouchDevice } from '@zondax/zemu'
import ArkApp from '@zondax/ledger-cosmos-js'
import { defaultOptions, DEVICE_MODELS } from './common'

// @ts-ignore
// import secp256k1 from 'secp256k1/elliptic'

jest.setTimeout(90000)

describe('Standard', function () {
  test.concurrent.each(DEVICE_MODELS)('can start and stop container', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('main menu', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const nav = zondaxMainmenuNavigation(m.name, [1, 0, 0, 4, -5])
      await sim.navigateAndCompareSnapshots('.', `${m.prefix.toLowerCase()}-mainmenu`, nav.schedule)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('get app version', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())
      const resp = await app.getVersion()

      console.log(resp)

      expect(resp).toHaveProperty('major')
      expect(resp).toHaveProperty('minor')
      expect(resp).toHaveProperty('patch')
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('get address', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())

      // Derivation path. First 3 items are automatically hardened!
      const path = "m/44'/330'/5'/0/3"
      const resp = await app.getAddressAndPubKey(path, 'ark')

      console.log(resp)

      expect(resp).toHaveProperty('bech32_address')
      expect(resp).toHaveProperty('compressed_pk')

      expect(resp.bech32_address).toEqual('ark1rpml0hh6kc8g6at2lsatzkd550yc2ngnxrdxgt')
      expect(resp.compressed_pk.length).toEqual(33)
      expect(resp.compressed_pk.toString("hex")).toEqual('0389ec5e88cd420accbb73b35096b6ab50b42c501422443f3158cbe5d5634ef851')
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('show address', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({
        ...defaultOptions,
        model: m.name,
        approveKeyword: isTouchDevice(m.name) ? 'Confirm' : '',
        approveAction: ButtonKind.DynamicTapButton,
      })
      const app = new ArkApp(sim.getTransport())

      // Derivation path. First 3 items are automatically hardened!
      const path = "m/44'/330'/5'/0/3"
      const respRequest = app.showAddressAndPubKey(path, 'ark')
      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-show_address`)

      const resp = await respRequest
      console.log(resp)

      expect(resp).toHaveProperty('bech32_address')
      expect(resp).toHaveProperty('compressed_pk')

      expect(resp.bech32_address).toEqual('ark1rpml0hh6kc8g6at2lsatzkd550yc2ngnxrdxgt')
      expect(resp.compressed_pk.length).toEqual(33)
      expect(resp.compressed_pk.toString("hex")).toEqual('0389ec5e88cd420accbb73b35096b6ab50b42c501422443f3158cbe5d5634ef851')
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('get legacy address', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())

      // Pre-2019 Terra wallets derived at coin type 118. The legacy
      // 118' path stays accepted so those holders can reach their accounts.
      const path = "m/44'/118'/0'/0/0"
      const resp = await app.getAddressAndPubKey(path, 'ark')

      console.log(resp)

      expect(resp).toHaveProperty('bech32_address')
      expect(resp).toHaveProperty('compressed_pk')

      expect(resp.bech32_address).toEqual('ark1w34k53py5v5xyluazqpq65agyajavep2nns3mh')
      expect(resp.compressed_pk.length).toEqual(33)
      expect(resp.compressed_pk.toString("hex")).toEqual('034fef9cd7c4c63588d3b03feb5281b9d232cba34d6f3d71aee59211ffbfe1fe87')
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('unsupported coin type is refused', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())

      // The app serves 330' and the legacy 118' only. The Ethereum-style 60'
      // derivation was removed with the ETH address support, so the path is
      // refused before the HRP is even considered.
      const path = "m/44'/60'/0'/0/1"

      await expect(app.getAddressAndPubKey(path, 'ark')).rejects.toMatchObject({
        returnCode: 0x698B,
      })

      // Same guard on the confirm-on-device entry point: it has to fail before
      // anything reaches the screen.
      await expect(app.showAddressAndPubKey(path, 'ark')).rejects.toMatchObject({
        returnCode: 0x698B,
      })

      // There is no HRP table: any well-formed HRP works on a supported path,
      // so wallets that re-encode the same key under another prefix keep
      // working.
      const resp = await app.getAddressAndPubKey("m/44'/330'/0'/0/0", 'osmo')
      expect(resp).toHaveProperty('bech32_address')
      expect(resp.bech32_address.startsWith('osmo1')).toBe(true)
      expect(resp.compressed_pk.length).toEqual(33)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('malformed HRP is refused', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())

      const path = "m/44'/330'/0'/0/0"

      // The HRP length is declared out of band, so the bytes in between do not
      // have to form a C string. The encoder measures the HRP with strlen, so
      // 'ark\0X' announced as 5 bytes would be truncated back to 'ark': the
      // device would answer under a different HRP than the one it validated.
      await expect(app.getAddressAndPubKey(path, 'ark\u0000X')).rejects.toMatchObject({
        returnCode: 0x698C,
        errorMessage: 'Chain config not supported'
      })

      // Same on the confirm-on-device entry point: nothing may reach the screen.
      await expect(app.showAddressAndPubKey(path, 'ark\u0000X')).rejects.toMatchObject({
        returnCode: 0x698C,
        errorMessage: 'Chain config not supported'
      })

      // A trailing NUL counted inside the declared length is the same bypass.
      await expect(app.getAddressAndPubKey(path, 'ark\u0000')).rejects.toMatchObject({
        returnCode: 0x698C,
        errorMessage: 'Chain config not supported'
      })

      // bech32 HRPs are printable lowercase ASCII. These were already refused,
      // but only once the encoder saw them -- two layers below the chain-config
      // decision, and reported as a generic execution error.
      for (const hrp of ['ARK', 'ArK', 'ar\u0001k', 'ar k']) {
        await expect(app.getAddressAndPubKey(path, hrp)).rejects.toMatchObject({
          returnCode: 0x698C,
          errorMessage: 'Chain config not supported'
        })
      }

      // Well-formed HRPs are untouched by any of the above, on both paths.
      const resp = await app.getAddressAndPubKey(path, 'ark')
      expect(resp).toHaveProperty('bech32_address')
      expect(resp.bech32_address.startsWith('ark1')).toBe(true)
      expect(resp.compressed_pk.length).toEqual(33)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('show address HUGE', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({
        ...defaultOptions,
        model: m.name,
        approveKeyword: isTouchDevice(m.name) ? 'Confirm' : '',
        approveAction: ButtonKind.DynamicTapButton,
      })
      const app = new ArkApp(sim.getTransport())

      // Derivation path. First 3 items are automatically hardened!
      const path = "m/44'/330'/2147483647'/0/4294967295"
      const resp = app.showAddressAndPubKey(path, 'ark')
      console.log(resp)

      await expect(resp).rejects.toMatchObject({
        returnCode: 0x6989,
        errorMessage: 'Invalid HD Path Value. Expert Mode required.'
      })
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(DEVICE_MODELS)('show address HUGE Expert', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({
        ...defaultOptions,
        model: m.name,
        approveKeyword: isTouchDevice(m.name) ? 'Confirm' : '',
        approveAction: ButtonKind.DynamicTapButton,
      })
      const app = new ArkApp(sim.getTransport())

      // Activate expert mode
      await sim.toggleExpertMode();

      // Derivation path. First 3 items are automatically hardened!
      const path = "m/44'/330'/2147483647'/0/4294967295"
      const respRequest = app.showAddressAndPubKey(path, 'ark')

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-show_address_huge`)

      const resp = await respRequest
      console.log(resp)

      expect(resp).toHaveProperty('bech32_address')
      expect(resp).toHaveProperty('compressed_pk')

      expect(resp.bech32_address).toEqual('ark1w546yt00vx7ed7c6kkrdy8df3r6rpln53sv2t7')
      expect(resp.compressed_pk.length).toEqual(33)
    } finally {
      await sim.close()
    }
  })
})
