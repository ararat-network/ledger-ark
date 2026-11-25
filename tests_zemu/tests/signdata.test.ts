// SPDX-License-Identifier: Apache-2.0

// Signup documents: the ADR-036 amino sign documents of the Ark member signup
// protocol (ark-app, signer/docs/SIGNUP_PROTOCOL.md, section 5). A member whose
// account is on a Ledger proves possession of their address, and vouches for
// another admission, by signing one of these on the device. The tests pin what
// each device shows for one and that the signature it returns verifies.
//
// The sign bytes are the protocol's own test vectors, byte for byte. The
// vectors were signed by test keys that are not the emulator's, so their
// signatures are not expected here: the device signs with its own key and the
// result is verified against its own public key over those exact bytes. The
// device does not compare the document's "signer" with its own address, so a
// vector naming another address is reviewed and signed like any other. The
// last test rebuilds a document with the device's address, which is what a
// member signs in practice, and checks the result against the protocol's
// verification rules 1 to 3.

import Zemu, { IDeviceModel } from '@zondax/zemu'
// @ts-ignore
import ArkApp from '@zondax/ledger-cosmos-js'
import { bech32 } from 'bech32'
import { defaultOptions, DEVICE_MODELS, AMINO_JSON_TX, signup_challenge_sign_bytes, signup_vouch_sign_bytes } from './common'

// @ts-ignore
import secp256k1 from 'secp256k1/elliptic'
// @ts-ignore
import crypto from 'crypto'

jest.setTimeout(180000)

const PATH = "m/44'/330'/0'/0/0"
const HRP = 'ark'

// The document inside a sign document's one message, after checking that the
// sign bytes have the shape section 5 of the protocol gives them. JSON.parse
// keeps the members in the order of the sign bytes, so a round trip that
// returns the same text shows the bytes are compact with their keys in order.
function signupDocument(signBytes: string): Record<string, string> {
  const doc = JSON.parse(signBytes)
  expect(JSON.stringify(doc)).toEqual(signBytes)

  const { msgs, ...envelope } = doc
  expect(envelope).toEqual({ account_number: '0', chain_id: '', fee: { amount: [], gas: '0' }, memo: '', sequence: '0' })
  expect(msgs).toHaveLength(1)
  expect(msgs[0].type).toEqual('sign/MsgSignData')
  expect(Object.keys(msgs[0].value)).toEqual(['data', 'signer'])

  return JSON.parse(Buffer.from(msgs[0].value.data, 'base64').toString('utf-8'))
}

// Has the device review and sign the bytes, comparing every screen with the
// named snapshot, and verifies the signature over those bytes with the device's
// own public key. Returns the signature as the protocol submits it, 64 bytes
// r || s, with the public key and address it belongs to.
async function reviewAndSign(m: IDeviceModel, signBytes: string, snapshot: string, expert: boolean) {
  const sim = new Zemu(m.path)
  try {
    await sim.start({ ...defaultOptions, model: m.name })
    const app = new ArkApp(sim.getTransport())

    if (expert) {
      await sim.toggleExpertMode()
    }

    const tx = Buffer.from(signBytes, 'utf-8')

    // get address / publickey
    const respPk = await app.getAddressAndPubKey(PATH, HRP)
    expect(respPk).toHaveProperty('compressed_pk')
    expect(respPk).toHaveProperty('bech32_address')

    // do not wait here..
    const signatureRequest = app.sign(PATH, tx, HRP, AMINO_JSON_TX)

    // Wait until we are not in the main menu
    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
    await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-${snapshot}`)

    const resp = await signatureRequest
    expect(resp).toHaveProperty('signature')

    // Now verify the signature
    const hash = crypto.createHash('sha256')
    const msgHash = Uint8Array.from(hash.update(tx).digest())

    const signatureDER = resp.signature
    const signature: Uint8Array = secp256k1.signatureImport(Uint8Array.from(signatureDER))

    const pk = Uint8Array.from(respPk.compressed_pk)

    const signatureOk = secp256k1.ecdsaVerify(signature, msgHash, pk)
    expect(signatureOk).toEqual(true)

    // The protocol refuses a signature whose s is in the upper half of the
    // curve order. The device answers in DER; converted to r || s it is already
    // in the lower half, so normalising it changes nothing.
    expect(signature).toHaveLength(64)
    expect(Buffer.from(secp256k1.signatureNormalize(Uint8Array.from(signature)))).toEqual(Buffer.from(signature))

    return { signature, pk, address: respPk.bech32_address as string }
  } finally {
    await sim.close()
  }
}

describe('Signup documents', function () {
  test('the sign bytes are the signup vectors', function () {
    expect(signupDocument(signup_challenge_sign_bytes).domain).toEqual('ark-signup/v1')
    expect(signupDocument(signup_vouch_sign_bytes).domain).toEqual('ark-vouch/v1')
  })

  // An empty chain_id is not the default chain, so normal mode hides nothing:
  // the review shows every field of the document that expert mode shows.
  test.concurrent.each(DEVICE_MODELS)('sign signup challenge', async function (m) {
    await reviewAndSign(m, signup_challenge_sign_bytes, 'sign_signup_challenge', false)
  })

  test.concurrent.each(DEVICE_MODELS)('sign signup challenge expert', async function (m) {
    await reviewAndSign(m, signup_challenge_sign_bytes, 'sign_signup_challenge_expert', true)
  })

  test.concurrent.each(DEVICE_MODELS)('sign signup vouch', async function (m) {
    await reviewAndSign(m, signup_vouch_sign_bytes, 'sign_signup_vouch', false)
  })

  test.concurrent.each(DEVICE_MODELS)('sign signup vouch expert', async function (m) {
    await reviewAndSign(m, signup_vouch_sign_bytes, 'sign_signup_vouch_expert', true)
  })

  // What a member signs: the challenge with their own address as the signer.
  // The vector's address is replaced with the device's and the sign bytes are
  // rebuilt in the same canonical form. JSON.stringify writes the members in
  // the order they were parsed, which is sorted, with no whitespace, and an
  // address holds nothing that Go's encoder would escape, so the bytes are the
  // ones the chain's JSON sorting gives.
  test.concurrent.each(DEVICE_MODELS)('sign signup challenge for the device address', async function (m) {
    const sim = new Zemu(m.path)
    let ownAddress: string
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new ArkApp(sim.getTransport())
      ownAddress = (await app.getAddressAndPubKey(PATH, HRP)).bech32_address
    } finally {
      await sim.close()
    }

    const doc = JSON.parse(signup_challenge_sign_bytes)
    const vectorAddress: string = doc.msgs[0].value.signer
    expect(ownAddress).not.toEqual(vectorAddress)
    expect(ownAddress).toHaveLength(vectorAddress.length)
    doc.msgs[0].value.signer = ownAddress
    const signBytes = JSON.stringify(doc)
    expect(signBytes).toEqual(signup_challenge_sign_bytes.replace(vectorAddress, ownAddress))

    const { signature, pk, address } = await reviewAndSign(m, signBytes, 'sign_signup_challenge_own_address', false)

    // Rule 1: the signer is lowercase Bech32, prefix ark, 20-byte payload.
    expect(address).toEqual(ownAddress)
    expect(address).toEqual(address.toLowerCase())
    const decoded = bech32.decode(address)
    expect(decoded.prefix).toEqual('ark')
    const payload = Buffer.from(bech32.fromWords(decoded.words))
    expect(payload).toHaveLength(20)

    // Rule 2: the public key is 33 bytes compressed and derives to the signer.
    expect(pk).toHaveLength(33)
    const sha = crypto.createHash('sha256').update(pk).digest()
    expect(crypto.createHash('ripemd160').update(sha).digest()).toEqual(payload)

    // Rule 3: 64 bytes, low-S, verifying over the sign bytes built from the
    // signer and the data. reviewAndSign has checked all three.
    expect(signature).toHaveLength(64)
  })
})
