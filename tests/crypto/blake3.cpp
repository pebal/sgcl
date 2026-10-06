//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// BLAKE3: the vectors of the authors' test_vectors.json (inputs of i % 251,
// the three modes, 131 bytes of output, made with their C library 1.8.7),
// the C library itself on random data fed in pieces and read from random
// positions when the build finds it (SGCL_TEST_BLAKE3), the four-lane path
// held against the portable one, and the edges of every member: a key of
// 31 and 33 bytes, nothing hashed, nothing read, positions across the
// counter's 32 bits, reset keeping the mode, a copy branching, verify, the
// destructor zeroing a keyed state.
#include "digest_common.h"

#include "sgcl/crypto/blake3.h"

#include <gtest/gtest.h>

#if defined(SGCL_TEST_BLAKE3)
#include <blake3.h>
#endif

#include <new>
#include <stdexcept>

using namespace crypto_test;

namespace {
    struct Kat {
        size_t length;
        const char* hash;
        const char* keyed;
        const char* derived;
    };

    const Kat kats[] = {
        {0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262e00f03e7b69af26b7faaf09fcd333050338ddfe085b8cc869ca98b206c08243a26f5487789e8f660afe6c99ef9e0c52b92e7393024a80459cf91f476f9ffdbda7001c22e159b402631f277ca96f2defdf1078282314e763699a31c5363165421cce14d",
         "921a8e7ec2add7bdd5e7a03455cd8c630af8ccf33c6b032c331088a7ad4b09c0b8e83b4ee09be5d383ff5e2060b349b1dc4ab1ef053b23b8c7fad942d8a4677da497a126dfc38334dfa679a305b182bb49609f505aa96ee134df094fce1fbdf1b56429286f78acb2e957f6ea8cde446ab9bc44aff9c6a037cbe69a70a767309f2e26b5",
         "2cc39783c223154fea8dfb7c1b1660f2ac2dcbd1c1de8277b0b0dd39b7e50d7d905630c8be290dfcf3e6842f13bddd573c098c3f17361f1f206b8cad9d088aa4a3f746752c6b0ce6a83b0da81d59649257cdf8eb3e9f7d4998e41021fac119deefb896224ac99f860011f73609e6e0e4540f93b273e56547dfd3aa1a035ba6689d89a0"},
        {1, "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213c3a6cb8bf623e20cdb535f8d1a5ffb86342d9c0b64aca3bce1d31f60adfa137b358ad4d79f97b47c3d5e79f179df87a3b9776ef8325f8329886ba42f07fb138bb502f4081cbcec3195c5871e6c23e2cc97d3c69a613eba131e5f1351f3f1da786545e5",
         "0b99f194755f20072e5b0c9739834d65177cc4968c3834b02e31a5a2ba95b8362981b75992c8d03fc5c2a1e9f9e85ab4e28cfbe5c304929db4a41b6ffb15cb27eac62da10c0b23dc2b66257409a651a576e9f8407bfb957e22dfc31e9da298d9ee98043203b94b7e0e4b7da61c639f017f9698f6a1001830d70dc2c3eb2d3eff379ca8",
         "b3e2e340a117a499c6cf2398a19ee0d29cca2bb7404c73063382693bf66cb06c5827b91bf889b6b97c5477f535361caefca0b5d8c4746441c57617111933158950670f9aa8a05d791daae10ac683cbef8faf897c84e6114a59d2173c3f417023a35d6983f2c7dfa57e7fc559ad751dbfb9ffab39c2ef8c4aafebc9ae973a64f0c76551"},
        {1023, "10108970eeda3eb932baac1428c7a2163b0e924c9a9e25b35bba72b28f70bd11a182d27a591b05592b15607500e1e8dd56bc6c7fc063715b7a1d737df5bad3339c56778957d870eb9717b57ea3d9fb68d1b55127bba6a906a4a24bbd5acb2d123a37b28f9e9a81bbaae360d58f85e5fc9d75f7c370a0cc09b6522d9c8d822f2f28f485",
         "017a5bfff9539f8c6d6156005488259bf2af47dd009ab87f5303cb13ee74d780e34291b0a2548c247064cdfa132d9f5cde33ed13f8b4280da4529236983c20e5d5fb2cd47be92d359a1f0c79458e4a2c412e949af549e89f5fb6f41573f2e9450f53347079c5f165b5043a1d6b4e8e9aeb91823b72e0072e0177effbeb76a27a4cc840",
         "74a16c1c3d44368a86e1ca6df64be6a2f64cce8f09220787450722d85725dea59c413264404661e9e4d955409dfe4ad3aa487871bcd454ed12abfe2c2b1eb7757588cf6cb18d2eccad49e018c0d0fec323bec82bf1644c6325717d13ea712e6840d3e6e730d35553f59eff5377a9c350bcc1556694b924b858f329c44ee64b884ef00d"},
        {1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af71cf8107265ecdaf8505b95d8fcec83a98a6a96ea5109d2c179c47a387ffbb404756f6eeae7883b446b70ebb144527c2075ab8ab204c0086bb22b7c93d465efc57f8d917f0b385c6df265e77003b85102967486ed57db5c5ca170ba441427ed9afa684e",
         "f048e764affbccc6d62234081bf2e7d1db8fcff0691dcfc79638728a265931172daf22d5b9359eb3f37d25f66bb55a466c5a5b961401b011675778f2431e096336437aea2ea8d5336947b1cfaaa52d22d8c5373ef1de4400bb83e6ee27cf725235e2a086429b5250079bcbba8d8a45960c9071aff8a45f6306de664b4ecc6838e9a96f",
         "7356cd7720d5b66b6d0697eb3177d9f8d73a4a5c5e968896eb6a6896843027066c23b601d3ddfb391e90d5c8eccdef4ae2a264bce9e612ba15e2bc9d654af1481b2e75dbabe615974f1070bba84d56853265a34330b4766f8e75edd1f4a1650476c10802f22b64bd3919d246ba20a17558bc51c199efdec67e80a227251808d8ce5bad"},
        {1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444f4c4a22b4b399155358a994e52bf255de60035742ec71bd08ac275a1b51cc6bfe332b0ef84b409108cda080e6269ed4b3e2c3f7d722aa4cdc98d16deb554e5627be8f955c98e1d5f9565a9194cad0c4285f93700062d9595adb992ae68ff12800ab67a",
         "3dcb54b3f15625e4fa7b92be8aea2a80bac2edbce9157ca9f1f6c8205bbbbb8aa50a849dc4dc39f822dda9a47d40212ee513a154ec8bdf7b5869f1ab45fa2afd245b8f72e3cdb3b0a962dfbca015a148be4e17a932d27dda9a824b633fe8b8ead9cea0a2b5e5bc88f4b02a29161b7a7d3e682906a045feeed964844eb4451a161736ba",
         "effaa245f065fbf82ac186839a249707c3bddf6d3fdda22d1b95a3c970379bcb5d31013a167509e9066273ab6e2123bc835b408b067d88f96addb550d96b6852dad38e320b9d940f86db74d398c770f462118b35d2724efa13da97194491d96dd37c3c09cbef665953f2ee85ec83d88b88d11547a6f911c8217cca46defa2751e7f3ad"},
        {2048, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576467e838ed6f2529b85fba24a9a60bf80001410ec9eea6698cd537939fad4749edd484cb541aced55cd9bf54764d063f23f6f1e32e12958ba5cfeb1bf618ad094266d4fc3c968c2088f677454c288c67ba0dba337b9d91c7e1ba586dc9a5bc2d5e90c14f53a8863ac75655461cea8f9",
         "814de947150881e47f7fe43dc71a818763cc8ce495f84148330be42f35c5b19961bae2ea8d561b20ba2865e3f45c13c620ecb7940c10c6602c4315304cd209adeab7d634d4b017198ae3edb3d71ebf9cfd4f3825438a7f00808d4fdf7b0151c30baddbcc44de7861b03698aa3db2c52981166972e75858b4bc28211d23000e8b8192a2",
         "7b2945cb4fef70885cc5d78a87bf6f6207dd901ff239201351ffac04e1088a23e2c11a1ebffcea4d80447867b61badb1383d842d4e79645d48dd82ccba290769caa7af8eaa1bd78a2a5e6e94fbdab78d9c7b74e894879f6a515257ccf6f95056f4e25390f24f6b35ffbb74b766202569b1d797f2d4bd9d17524c720107f985f4ddc583"},
        {2049, "5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c426c95c1af0b687952256303096de31d71d74103403822a2e0bc1eb193e7aecc9643a76b7bbc0c9f9c52e8783aae98764ca468962b5c2ec92f0c74eb5448d519713e09413719431c802f948dd5d90425a4ecdadece9eb178d80f26efccae630734dff63340285adec2aed3b51073ad3",
         "a8d5004129f6e1142b05f7a47b62c9d940cb30d0aefa9073a1f7b17eeeefa87d1b0fb3871236918e37a634f73115a06c01b8dce8f3c17b7ec911216dddd353f54b1dadb6344c83c9d7755cfa678238af49655caab1737242f8d7cf8c75b7bdcb7abea8822e398a1381038357def29f1a0f08332a52df1a146a7c6351460c8390e0f670",
         "2ea477c5515cc3dd606512ee72bb3e0e758cfae7232826f35fb98ca1bcbdf27316d8e9e79081a80b046b60f6a263616f33ca464bd78d79fa18200d06c7fc9bffd808cc4755277a7d5e09da0f29ed150f6537ea9bed946227ff184cc66a72a5f8c1e4bd8b04e81cf40fe6dc4427ad5678311a61f4ffc39d195589bdbc670f63ae70f4b6"},
        {3072, "b98cb0ff3623be03326b373de6b9095218513e64f1ee2edd2525c7ad1e5cffd29a3f6b0b978d6608335c09dc94ccf682f9951cdfc501bfe47b9c9189a6fc7b404d120258506341a6d802857322fbd20d3e5dae05b95c88793fa83db1cb08e7d8008d1599b6209d78336e24839724c191b2a52a80448306e0daa84a3fdb566661a37e11",
         "1c8c4a3f1a9d172184a6edc73a15848891006d6469ddb690196882730396d258fb5b59040a99f3fc452d497383687908c64c9f3ace891913f06775158d3a2d0b4698734c902e52036589a9e1ed81fd3ec6e3515a42f93d75d4bdb8b6addfd368c9962378fcd1361f240906c44fff0ae53aa01413336a5a97b0b9de9ee89bd69db00075",
         "050df97f8c2ead654d9bb3ab8c9178edcd902a32f8495949feadcc1e0480c46b3604131bbd6e3ba573b6dd682fa0a63e5b165d39fc43a625d00207607a2bfeb65ff1d29292152e26b298868e3b87be95d6458f6f2ce6118437b632415abe6ad522874bcd79e4030a5e7bad2efa90a7a7c67e93f0a18fb28369d0a9329ab5c24134ccb0"},
        {3073, "7124b49501012f81cc7f11ca069ec9226cecb8a2c850cfe644e327d22d3e1cd39a27ae3b79d68d89da9bf25bc27139ae65a324918a5f9b7828181e52cf373c84f35b639b7fccbb985b6f2fa56aea0c18f531203497b8bbd3a07ceb5926f1cab74d14bd66486d9a91eba99059a98bd1cd25876b2af5a76c3e9eed554ed72ea952b603bf",
         "3891af19918d51ffd211b583b818322974fe0f253ca8597784dad2f1c7e0e514e327ae4d2a0c74ab608250a5ac2eaf8f4245c6674fdd35e6ac2b6875cf15066daf7c0c5e5a199834ae651c5ec2720d1614ead1e0488e899f9808dcc85cdb181250718247af483dcf51a3572e3e68e787eacec02b4de806ef7b973fded2fe9bb3068668",
         "72613c9ec9ff7e40f8f5c173784c532ad852e827dba2bf85b2ab4b76f7079081576288e552647a9d86481c2cae75c2dd4e7c5195fb9ada1ef50e9c5098c249d743929191441301c69e1f48505a4305ec1778450ee48b8e69dc23a25960fe33070ea549119599760a8a2d28aeca06b8c5e9ba58bc19e11fe57b6ee98aa44b2a8e6b14a5"},
        {4096, "015094013f57a5277b59d8475c0501042c0b642e531b0a1c8f58d2163229e9690289e9409ddb1b99768eafe1623da896faf7e1114bebeadc1be30829b6f8af707d85c298f4f0ff4d9438aef948335612ae921e76d411c3a9111df62d27eaf871959ae0062b5492a0feb98ef3ed4af277f5395172dbe5c311918ea0074ce0036454f620",
         "06ffac7e8e58cba7fa545ff239cf6dd860b2281fadd558c34ad32d5dbd1310812f382f87ec5c0762bb4c7b00f9fb8ed54e9741f361e0a19f9234b3e863d6129f365f7e157eec2be173ee03c084519558591209c9aedff06a199ee0904ce085d718c3459e050b474c3915cc5aca00c02ab2033fc97f7388f15f4ae275babc26e877a614",
         "1e0d7f3db8c414c97c6307cbda6cd27ac3b030949da8e23be1a1a924ad2f25b9d78038f7b198596c6cc4a9ccf93223c08722d684f240ff6569075ed81591fd93f9fff1110b3a75bc67e426012e5588959cc5a4c192173a03c00731cf84544f65a2fb9378989f72e9694a6a394a8a30997c2e67f95a504e631cd2c5f55246024761b245"},
        {4097, "9b4052b38f1c5fc8b1f9ff7ac7b27cd242487b3d890d15c96a1c25b8aa0fb99505f91b0b5600a11251652eacfa9497b31cd3c409ce2e45cfe6c0a016967316c426bd26f619eab5d70af9a418b845c608840390f361630bd497b1ab44019316357c61dbe091ce72fc16dc340ac3d6e009e050b3adac4b5b2c92e722cffdc46501531956",
         "81c775a84a4c584a02dcd7d25747568007f31216be36edb37b5c3332ecb120991ef8f8728cd2fac6417a55175e2b1191520cf256e1e29690dcd907665d7f99a9c7137be004721eea731501009cd088a8744dc2dd80e6e1c686e2655c4759847f2c9fea616bb4d99c0187907e24be1775f78fceb2386be3f06a4fb84273dd65b7413261",
         "aca51029626b55fda7117b42a7c211f8c6e9ba4fe5b7a8ca922f34299500ead8a897f66a400fed9198fd61dd2d58d382458e64e100128075fc54b860934e8de2e84170734b06e1d212a117100820dbc48292d148afa50567b8b84b1ec336ae10d40c8c975a624996e12de31abbe135d9d159375739c333798a80c64ae895e51e22f3ad"},
        {5120, "9cadc15fed8b5d854562b26a9536d9707cadeda9b143978f319ab34230535833acc61c8fdc114a2010ce8038c853e121e1544985133fccdd0a2d507e8e615e611e9a0ba4f47915f49e53d721816a9198e8b30f12d20ec3689989175f1bf7a300eee0d9321fad8da232ece6efb8e9fd81b42ad161f6b9550a069e66b11b40487a5f5059",
         "335cc13c953993b8d861103a8df147a76dcb14f877c2d65a1fa527e84ecd6a42d7e66e0cf390f1e0e80cf578dcecb846e6f78b5e39332e571721e26c6d1cee0dde8efb4a6fb742ed156ed39e49f4b78cc325f3d4ede6e4dfebc7d5cdd1644f157f68d91fba96a4f4404eecbf0e4c67fd377bce40f0bcda6867812cbf61f4b8f071de45",
         "7a7acac8a02adcf3038d74cdd1d34527de8a0fcc0ee3399d1262397ce5817f6055d0cefd84d9d57fe792d65a278fd20384ac6c30fdb340092f1a74a92ace99c482b28f0fc0ef3b923e56ade20c6dba47e49227166251337d80a037e987ad3a7f728b5ab6dfafd6e2ab1bd583a95d9c895ba9c2422c24ea0f62961f0dca45cad47bfa0d"},
        {5121, "628bd2cb2004694adaab7bbd778a25df25c47b9d4155a55f8fbd79f2fe154cff96adaab0613a6146cdaabe498c3a94e529d3fc1da2bd08edf54ed64d40dcd6777647eac51d8277d70219a9694334a68bc8f0f23e20b0ff70ada6f844542dfa32cd4204ca1846ef76d811cdb296f65e260227f477aa7aa008bac878f72257484f2b6c95",
         "dcf95973a556860da7922dd264beb4cb6e69446daff9b7bb2e5c915639b0246ea16a7554495da8ab8111fc4117267e7087a0a2caaa0d9ffcdbfb83dad26353d4976a8437a65a067230f5bb5d3ad4e6d6090e46f43500639e1c9a253d0a9b3dbe12666b9378a6d7fd911eaff0bb6d8e56dd4930ee3a4af915abe7f1a6225dd0839c629b",
         "b07f01e518e702f7ccb44a267e9e112d403a7b3f4883a47ffbed4b48339b3c341a0add0ac032ab5aaea1e4e5b004707ec5681ae0fcbe3796974c0b1cf31a194740c14519273eedaabec832e8a784b6e7cfc2c5952677e6c3f2c3914454082d7eb1ce1766ac7d75a4d3001fc89544dd46b5147382240d689bbbaefc359fb6ae30263165"},
        {6144, "3e2e5b74e048f3add6d21faab3f83aa44d3b2278afb83b80b3c35164ebeca2054d742022da6fdda444ebc384b04a54c3ac5839b49da7d39f6d8a9db03deab32aade156c1c0311e9b3435cde0ddba0dce7b26a376cad121294b689193508dd63151603c6ddb866ad16c2ee41585d1633a2cea093bea714f4c5d6b903522045b20395c83",
         "ab182601e1483a7ab75e24875f9bdb4621ac90790ee3fa6b2f89948c1f9b21cf27511ea74cc2c3595cd13f2d958f60ca6560e466cc111b30488f5d7da84431cb1a741f53f7f0c050381fe166fc25e66d09497351e527eba9ea7c969c8a356e70fdbc1b73dc3fdd45c21da8532e75a248d35b5300eb04c8baf41baa1c2470662f29c588",
         "2a95beae63ddce523762355cf4b9c1d8f131465780a391286a5d01abb5683a1597099e3c6488aab6c48f3c15dbe1942d21dbcdc12115d19a8b8465fb54e9053323a9178e4275647f1a9927f6439e52b7031a0b465c861a3fc531527f7758b2b888cf2f20582e9e2c593709c0a44f9c6e0f8b963994882ea4168827823eef1f64169fef"},
        {6145, "f1323a8631446cc50536a9f705ee5cb619424d46887f3c376c695b70e0f0507f18a2cfdd73c6e39dd75ce7c1c6e3ef238fd54465f053b25d21044ccb2093beb015015532b108313b5829c3621ce324b8e14229091b7c93f32db2e4e63126a377d2a63a3597997d4f1cba59309cb4af240ba70cebff9a23d5e3ff0cdae2cfd54e070022",
         "5f3fe00fb8a6c598e356ee0f0beab0bbf7a667c6539cd55d2b771229026145f821d58fa9fcfe1ad3ff722b5b2a16fbd765dec7586e470258d882f9bc7ed6e47095b6df556dab1906ca8249e7ec333e175ed20c504fbe2b29af3bb90b91c1e328cf40a29d4d309a91417e37337a9e2b0fb2b9e659f37be92bce3ffaa142172ad343b960",
         "379bcc61d0051dd489f686c13de00d5b14c505245103dc040d9e4dd1facab8e5114493d029bdbd295aaa744a59e31f35c7f52dba9c3642f773dd0b4262a9980a2aef811697e1305d37ba9d8b6d850ef07fe41108993180cf779aeece363704c76483458603bbeeb693cffbbe5588d1f3535dcad888893e53d977424bb707201569a8d2"},
        {7168, "61da957ec2499a95d6b8023e2b0e604ec7f6b50e80a9678b89d2628e99ada77a5707c321c83361793b9af62a40f43b523df1c8633cecb4cd14d00bdc79c78fca5165b863893f6d38b02ff7236c5a9a8ad2dba87d24c547cab046c29fc5bc1ed142e1de4763613bb162a5a538e6ef05ed05199d751f9eb58d332791b8d73fb74e4fce95",
         "d1beb287875a674fb4d2cb4842f4802d67893e13d37cdbe18539f52f940730261180d3350f8041afd3499de6d5fcf06fe73462c4b43c5a652816b0b01b029aed1eb7a687c35f6a7e77e25fff6cb6425326824d9b40769537a97ee1f8799bdc04baee687621ed737e9208677b053d02fa580aac54adff02a53d3d8f4c372196bd1f5c52",
         "11c37a112765370c94a51415d0d651190c288566e295d505defdad895dae223730d5a5175a38841693020669c7638f40b9bc1f9f39cf98bda7a5b54ae24218a800a2116b34665aa95d846d97ea988bfcb53dd9c055d588fa21ba78996776ea6c40bc428b53c62b5f3ccf200f647a5aae8067f0ea1976391fcc72af1945100e2a6dcb88"},
        {7169, "a003fc7a51754a9b3c7fae0367ab3d782dccf28855a03d435f8cfe74605e781798a8b20534be1ca9eb2ae2df3fae2ea60e48c6fb0b850b1385b5de0fe460dbe9d9f9b0d8db4435da75c601156df9d047f4ede008732eb17adc05d96180f8a73548522840779e6062d643b79478a6e8dbce68927f36ebf676ffa7d72d5f68f050b119c8",
         "4e4da84dd07b4e4ca1e7b68df8ee9cf00869c9173006a80c9f81d2f3b3223561a66a43548b9179a6768c3a761b044912627e143ac21a22c2a039b7970b72888ec293e84e43a3ad6145b09d977c5430c9999c6f8843c821213856e97b63dfb7363d8ad6516477ce4c4a56d52e553767e142a3bac6b1b4bf711cd29b48b53a77b00112bb",
         "554b0a5efea9ef183f2f9b931b7497995d9eb26f5c5c6dad2b97d62fc5ac31d99b20652c016d88ba2a611bbd761668d5eda3e568e940faae24b0d9991c3bd25a65f770b89fdcadabcb3d1a9c1cb63e69721cacf1ae69fefdcef1e3ef41bc5312ccc17222199e47a26552c6adc460cf47a72319cb5039369d0060eaea59d6c65130f1dd"},
        {8192, "aae792484c8efe4f19e2ca7d371d8c467ffb10748d8a5a1ae579948f718a2a635fe51a27db045a567c1ad51be5aa34c01c6651c4d9b5b5ac5d0fd58cf18dd61a47778566b797a8c67df7b1d60b97b19288d2d877bb2df417ace009dcb0241ca1257d62712b6a4043b4ff33f690d849da91ea3bf711ed583cb7b7a7da2839ba71309bbf",
         "e3c3d7055911c5d4a0bef6fbd8d1409a1900328fe11843c8b36ae8ada190d2f0d3060f5cce489d5846b3cbef334f72489d5432b4e3e72b00080444ff281d57b2675088f36ffc96a034235f8ae91f8811cfd1ca6809fe1013bd2fd7ad0f25bcd9ad5260fc64b84b7c0f5da87c96502479bf46a46d166b90af4a828b9a08e67b74946483",
         "ad01d7ae4ad059b0d33baa3c01319dcf8088094d0359e5fd45d6aeaa8b2d0c3d4c9e58958553513b67f84f8eac653aeeb02ae1d5672dcecf91cd9985a0e67f4501910ecba25555395427ccc7241d70dc21c190e2aadee875e5aae6bf1912837e53411dabf7a56cbf8e4fb780432b0d7fe6cec45024a0788cf5874616407757e9e6bef7"},
        {8193, "bab6c09cb8ce8cf459261398d2e7aef35700bf488116ceb94a36d0f5f1b7bc3bb2282aa69be089359ea1154b9a9286c4a56af4de975a9aa4a5c497654914d279bea60bb6d2cf7225a2fa0ff5ef56bbe4b149f3ed15860f78b4e2ad04e158e375c1e0c0b551cd7dfc82f1b155c11b6b3ed51ec9edb30d133653bb5709d1dbd55f4e1ff6",
         "f1e5d89c7e965ec722dcf6188526bd7cd4c22b30b8ab62ffc6fde3830bd3d820890d91898cdbdb6b4677e8bedfa34df0d53bfeacdbaf2190cf39035e078866052a8d117c4b32e6dcdcec1d756189a13f022e312f305b0b33ac47b214bf71f283f198398cd989207efebe3230e1c9856619e38c48a082f64173c53f8dd17650ee481346",
         "af1e0346e389b17c23200270a64aa4e1ead98c61695d917de7d5b00491c9b0f12f20a01d6d622edf3de026a4db4e4526225debb93c1237934d71c7340bb5916158cbdafe9ac3225476b6ab57a12357db3abbad7a26c6e66290e44034fb08a20a8d0ec264f309994d2810c49cfba6989d7abb095897459f5425adb48aba07c5fb3c83c0"},
        {16384, "f875d6646de28985646f34ee13be9a576fd515f76b5b0a26bb324735041ddde49d764c270176e53e97bdffa58d549073f2c660be0e81293767ed4e4929f9ad34bbb39a529334c57c4a381ffd2a6d4bfdbf1482651b172aa883cc13408fa67758a3e47503f93f87720a3177325f7823251b85275f64636a8f1d599c2e49722f42e93893",
         "7c524bf00f0701f703f7cdcdb1be2211ee27b45df860bb2dfc75f4b4ac979cd621ebc93e2bd665253008f862c155e5fa53beab1a78c678bfd1e6933df94b11d124bfddde0b64bd9182b21485de1ed5de894576f3bd573a71b908a177d8e19f5fbd1ea18fc03292fb4afbd58a5edd487571ab95adb90a4517fdd13d3eeed849f15abf77",
         "160e18b5878cd0df1c3af85eb25a0db5344d43a6fbd7a8ef4ed98d0714c3f7e160dc0b1f09caa35f2f417b9ef309dfe5ebd67f4c9507995a531374d099cf8ae317542e885ec6f589378864d3ea98716b3bbb65ef4ab5e0ab5bb298a501f19a41ec19af84a5e6b428ecd813b1a47ed91c9657c3fba11c406bc316768b58f6802c9e9b57"},
        {31744, "62b6960e1a44bcc1eb1a611a8d6235b6b4b78f32e7abc4fb4c6cdcce94895c47860cc51f2b0c28a7b77304bd55fe73af663c02d3f52ea053ba43431ca5bab7bfea2f5e9d7121770d88f70ae9649ea713087d1914f7f312147e247f87eb2d4ffef0ac978bf7b6579d57d533355aa20b8b77b13fd09748728a5cc327a8ec470f4013226f",
         "e9d4fa8252e4d3df41ffe5a82a2a134e574ad0cbac1eb1df2a6635927e30058ec8713f8a2d4a609c2f9b36e41d71ce4d8905c706b05aa6db58de66063ffac881f78c99732b7cd9d4fa3e31f12b4d98db159d7bbf0748da199d2eb7993fe30a9be0f042d23ee2af84ebf1fd54fdb1be8e8e5839232cd00f1b0ee106afb2e5616a1a4118",
         "39772aef80e0ebe60596361e45b061e8f417429d529171b6764468c22928e28e9759adeb797a3fbf771b1bcea30150a020e317982bf0d6e7d14dd9f064bc11025c25f31e81bd78a921db0174f03dd481d30e93fd8e90f8b2fee209f849f2d2a52f31719a490fb0ba7aea1e09814ee912eba111a9fde9d5c274185f7bae8ba85d300a2b"},
        {102400, "bc3e3d41a1146b069abffad3c0d44860cf664390afce4d9661f7902e7943e085e01c59dab908c04c3342b816941a26d69c2605ebee5ec5291cc55e15b76146e6745f0601156c3596cb75065a9c57f35585a52e1ac70f69131c23d611ce11ee4ab1ec2c009012d236648e77be9295dd0426f29b764d65de58eb7d01dd42248204f45f8e",
         "e5cbf23f395a7683854402aa981d8d52ad7bc44318c251f564ee35814beea6cf8677f69110b4b2ac535a28f040f08aaa31c1b8b90614e19ce7386502f686b073cff3f8ea0f7d32ade64cbc5b5675bdf2c833f8758735a7d255a40f23af8d96f6ae7536d3cdbd0513080f0db02e896a0b832a8fbfa8498d28a890308206082e88d805e5",
         "4652cff7a3f385a6103b5c260fc1593e13c778dbe608efb092fe7ee69df6e9c6d83a3e041bc3a48df2879f4a0a3ed40e7c961c73eff740f3117a0504c2dff4786d44fb17f1549eb0ba585e40ec29bf7732f0b7e286ff8acddc4cb1e23b87ff5d824a986458dcc6a04ac83969b80637562953df51ed1a7e90a7926924d2763778be8560"},
    };

    const char kat_key[] = "whats the Elephant here for? :-)";
    const char kat_context[] = "BLAKE3 2019-12-27 16:29:52 test vectors context";

    bytes_t kat_input(size_t n) {
        bytes_t v(n);
        for (size_t i = 0; i < n; ++i) {
            v[i] = (unsigned char)(i % 251);
        }
        return v;
    }

    bytes_t out_of(const crypto::blake3& h, size_t n, uint64_t position = 0) {
        bytes_t v(n);
        h.value_to(out_view(v), position);
        return v;
    }

    crypto::blake3 keyed() {
        return crypto::blake3(view(reinterpret_cast<const unsigned char*>(kat_key), 32));
    }
}

TEST(Crypto_Blake3, TestVectors) {
    for (const Kat& k : kats) {
        bytes_t in = kat_input(k.length);
        crypto::blake3 h;
        h.update(view(in));
        EXPECT_EQ(hex(out_of(h, 131)), k.hash) << k.length;
        EXPECT_EQ(hex(h.value()), std::string(k.hash, 64)) << k.length;
        crypto::blake3 m = keyed();
        m.update(view(in));
        EXPECT_EQ(hex(out_of(m, 131)), k.keyed) << k.length;
        crypto::blake3 d = crypto::blake3::for_derive_key(kat_context);
        d.update(view(in));
        EXPECT_EQ(hex(out_of(d, 131)), k.derived) << k.length;
        EXPECT_EQ(hex(crypto::blake3::derive_key(kat_context, view(in), 131)), k.derived) << k.length;
        EXPECT_EQ(hex(crypto::blake3::of(view(in))), std::string(k.hash, 64));
        EXPECT_EQ(hex(crypto::blake3::of(view(in), view(reinterpret_cast<const unsigned char*>(kat_key), 32))), std::string(k.keyed, 64));
        // fed a byte, then in pieces of 1000: the same
        crypto::blake3 pieces;
        for (size_t i = 0; i < in.size();) {
            size_t take = i == 0 ? 1 : std::min<size_t>(1000, in.size() - i);
            pieces.update(view(in.data() + i, take));
            i += take;
        }
        EXPECT_EQ(hex(pieces.value()), std::string(k.hash, 64)) << k.length;
        // the output read from the middle
        for (size_t at : {1, 63, 64, 65, 100}) {
            EXPECT_EQ(hex(out_of(h, 131 - at, at)), std::string(k.hash + 2 * at)) << k.length << " at " << at;
        }
    }
}

TEST(Crypto_Blake3, Edges) {
    bytes_t k31(31, 1), k32(32, 1), k33(33, 1);
    EXPECT_THROW(crypto::blake3(view(k31)), std::invalid_argument);
    EXPECT_THROW(crypto::blake3(view(k33)), std::invalid_argument);
    EXPECT_THROW(crypto::blake3::of("x", view(k33)), std::invalid_argument);
    EXPECT_THROW(crypto::blake3(sgcl::slice<const byte>()), std::invalid_argument);
    EXPECT_NO_THROW(crypto::blake3(view(k32)));
    // nothing read: nothing written
    crypto::blake3 h;
    h.value_to(sgcl::slice<byte>());
    // reset keeps the mode
    crypto::blake3 m = keyed();
    m.update("some input");
    m.reset();
    EXPECT_EQ(hex(m.value()), std::string(kats[0].keyed, 64));
    crypto::blake3 d = crypto::blake3::for_derive_key(kat_context);
    d.update("x");
    d.reset();
    EXPECT_EQ(hex(d.value()), std::string(kats[0].derived, 64));
    // a copy branches; assignment and self-assignment
    bytes_t in = kat_input(5121);
    crypto::blake3 a;
    a.update(view(in.data(), 3000));
    crypto::blake3 b = a;
    a.update(view(in.data() + 3000, 2121));
    b.update("other");
    EXPECT_EQ(hex(a.value()), std::string(kats[12].hash, 64));
    EXPECT_NE(hex(b.value()), hex(a.value()));
    b = a;
    auto& self = b;
    b = self;
    EXPECT_EQ(hex(b.value()), hex(a.value()));
    EXPECT_EQ(hex(a.digest()), hex(a.value()));
    // derive_key of nothing asked
    EXPECT_EQ(crypto::blake3::derive_key("ctx", "material", 0).size(), 0u);
}

TEST(Crypto_Blake3, Verify) {
    crypto::blake3 m = keyed();
    bytes_t in = kat_input(1025);
    m.update(view(in));
    bytes_t tag = unhex(std::string(kats[4].keyed, 64));
    EXPECT_TRUE(m.verify(view(tag)));
    EXPECT_TRUE(m.verify(view(tag.data(), 16)));   // a prefix of the output is a tag of its length
    bytes_t long_tag = unhex(std::string(kats[4].keyed, 128));
    EXPECT_TRUE(m.verify(view(long_tag)));
    tag[5] ^= 1;
    EXPECT_FALSE(m.verify(view(tag)));
    EXPECT_FALSE(m.verify(sgcl::slice<const byte>()));
    bytes_t too_long = unhex(std::string(kats[4].keyed, 2 * 65 + 0));
    EXPECT_FALSE(m.verify(view(too_long)));
}

TEST(Crypto_Blake3, KeyedStateZeroedByTheDestructor) {
    for (int mode = 0; mode < 2; ++mode) {
        alignas(crypto::blake3) unsigned char storage[sizeof(crypto::blake3)];
        crypto::blake3* h = mode == 0 ? new (storage) crypto::blake3(keyed())
                                      : new (storage) crypto::blake3(crypto::blake3::for_derive_key("context"));
        bytes_t in = kat_input(5000);
        h->update(view(in));
        h->~blake3();
        bool zero = true;
        for (unsigned char c : storage) {
            zero = zero && c == 0;
        }
        EXPECT_TRUE(zero) << mode;
    }
}

TEST(Crypto_Blake3, PathsAgree) {
    // the four-lane hash_many against the portable one, chunks and parents,
    // counters across 2^32
    random_source r(3);
    bytes_t in = r.bytes(8 * 1024);
    uint32_t key[8];
    for (auto& w : key) {
        w = uint32_t(r.below(1u << 31));
    }
    const unsigned char* inputs[8];
    for (int i = 0; i < 8; ++i) {
        inputs[i] = in.data() + 1024 * i;
    }
    for (uint64_t counter : {uint64_t(0), uint64_t(5), uint64_t(0xfffffffe), uint64_t(1) << 40}) {
        for (size_t count : {1, 2, 3, 4, 5, 7, 8}) {
            unsigned char a[8 * 32], b[8 * 32];
            crypto::detail::blake3_hash_many(inputs, count, 16, key, counter, true, 16, 1, 2, a);
            crypto::detail::blake3_hash_many_portable(inputs, count, 16, key, counter, true, 16, 1, 2, b);
            EXPECT_EQ(hex(a, count * 32), hex(b, count * 32)) << count << " " << counter;
            crypto::detail::blake3_hash_many(inputs, count, 1, key, 0, false, 4, 0, 0, a);
            crypto::detail::blake3_hash_many_portable(inputs, count, 1, key, 0, false, 4, 0, 0, b);
            EXPECT_EQ(hex(a, count * 32), hex(b, count * 32)) << count;
        }
    }
}

#if defined(SGCL_TEST_BLAKE3)
TEST(Crypto_Blake3, AgainstTheCLibrary) {
    random_source r(2026);
    for (size_t n : {0, 1, 64, 1024, 1025, 4096, 4097, 65536, 65537, 100000, 1 << 20, (1 << 20) + 1023, 3 << 20}) {
        bytes_t in = r.bytes(n);
        bytes_t key = r.bytes(32);
        for (int mode = 0; mode < 3; ++mode) {
            blake3_hasher ref;
            crypto::blake3 h = mode == 0 ? crypto::blake3() : mode == 1 ? crypto::blake3(view(key)) : crypto::blake3::for_derive_key("a context");
            if (mode == 0) {
                blake3_hasher_init(&ref);
            } else if (mode == 1) {
                blake3_hasher_init_keyed(&ref, key.data());
            } else {
                blake3_hasher_init_derive_key(&ref, "a context");
            }
            blake3_hasher_update(&ref, in.data(), n);
            for (size_t i = 0; i < n;) {
                size_t take = std::min(n - i, 1 + r.below(n < 5000 ? 200 : 300000));
                h.update(view(in.data() + i, take));
                i += take;
            }
            // positions across the counter's low word: 2^32 blocks of 64 bytes
            for (uint64_t at : {uint64_t(0), uint64_t(1), uint64_t(1000), (uint64_t(1) << 38) - 100, uint64_t(1) << 40}) {
                size_t len = 1 + r.below(700);
                bytes_t expected(len);
                blake3_hasher_finalize_seek(&ref, at, expected.data(), len);
                EXPECT_EQ(hex(out_of(h, len, at)), hex(expected)) << n << " " << mode << " " << at;
            }
        }
    }
}
#endif
