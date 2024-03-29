package com.mercury0114.vocabulary;

import static com.mercury0114.vocabulary.FilesManager.readLinesAndSort;
import static com.mercury0114.vocabulary.QuestionAnswer.extractQuestionAnswer;
import static com.mercury0114.vocabulary.TextSelector.DEFAULT_COLOR_CODE;
import static com.mercury0114.vocabulary.TextSelector.extractChosenTexts;
import static com.mercury0114.vocabulary.TextSelector.getToggledColorCode;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.LinearLayout;
import androidx.appcompat.app.AppCompatActivity;
import com.google.common.collect.ImmutableList;
import com.mercury0114.vocabulary.QuestionAnswer.Column;
import java.io.File;

public class SelectCustomWordsActivity extends AppCompatActivity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.select_custom_content_layout);
    String filePath = getIntent().getStringExtra("PATH");
    Column column = Column.valueOf(getIntent().getStringExtra("COLUMN"));
    prepareButtons(filePath, column);
  }

  private void prepareButtons(String filePath, Column column) {
    final Button startButton = (Button) findViewById(R.id.start_practice_button_id);
    ImmutableList<Button> selectTextButtons = createSelectTextButtons(column, filePath);
    startButton.setOnClickListener(
        new OnClickListener() {
          public void onClick(View view) {
            ImmutableList<String> texts = extractChosenTexts(selectTextButtons);
            Intent intent = buildIntentForVocabularyActivity(column, texts, filePath);
            startActivity(intent);
          }
        });
  }

  private Intent buildIntentForVocabularyActivity(
      Column column, ImmutableList<String> texts, String filePath) {
    Intent intent = new Intent(SelectCustomWordsActivity.this, VocabularyActivity.class);
    intent.putExtra("TEXTS", texts);
    intent.putExtra("COLUMN", column.name());
    intent.putExtra("PATH", filePath);
    return intent;
  }

  private ImmutableList<Button> createSelectTextButtons(Column column, String filePath) {
    LinearLayout linearLayout = (LinearLayout) findViewById(R.id.select_custom_content_id);
    ImmutableList.Builder<Button> buttonsBuilder = new ImmutableList.Builder<Button>();
    for (String line : readLinesAndSort(new File(filePath))) {
      Button selectButton = createSelectButton(line, column);
      buttonsBuilder.add(selectButton);
      linearLayout.addView(selectButton);
    }
    return buttonsBuilder.build();
  }

  private Button createSelectButton(String line, Column column) {
    Button button = new Button(this);
    QuestionAnswer questionAnswer = extractQuestionAnswer(line, column);
    button.setText(questionAnswer.question);
    button.setContentDescription(line);
    button.setTextColor(DEFAULT_COLOR_CODE);
    button.setOnClickListener(
        new OnClickListener() {
          public void onClick(View view) {
            int toggledColorCode = getToggledColorCode(button);
            button.setTextColor(toggledColorCode);
          }
        });
    return button;
  }
}
