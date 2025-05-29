## MML VST Plugin

## 概要
MML VST Pluginは、Music Macro Language (MML) テキストをMIDIデータへ変換し、Cubase 14などのDAWで利用できるVSTプラグインです。
MML入力用のエディタUIを備え、入力したMMLを即座にMIDIへ変換・出力できます。

## 主な機能
- MMLテキストのパース・MIDI変換
- Cubase 14最適化
- VSTプラグインとして動作
- 入力エディタUI搭載（MML入力、変換ボタン、ステータス表示）

## ディレクトリ構成
```
Source/
  ├─ MMLPluginProcessor.*   // メイン処理・MIDI変換
  ├─ MMLPluginEditor.*      // MML入力UI
  └─ MMLParser/
        ├─ EnhancedMMLParser.* // MMLパーサ・MIDI生成ロジック
```

## ビルド方法
1. Visual Studio 2022で `MML.jucer` を開く
2. 必要に応じてJUCEライブラリをセットアップ
3. Debug/ReleaseビルドでVST3プラグインを生成

## 使い方
1. DAW（例：Cubase 14）に本プラグインをVSTとして読み込む
2. プラグインUIでMMLを入力し、変換ボタンを押す
3. MIDIトラックにMIDIデータが反映される

## 貢献・ライセンス
- Issue・プルリク歓迎
- ライセンス: MIT（必要に応じて記載）